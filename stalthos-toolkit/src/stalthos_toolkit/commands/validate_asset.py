"""`validate_asset` -- canonical pipeline stage 8 (quality assurance).

Acceptance run over a manifest and the artifacts it references. **Blender is
never required or invoked**: every geometry/LOD/texture fact is read from what
earlier stages already measured and recorded. A check that cannot be evaluated
offline reports `warn`/`skip`, never `pass` (docs/toolkit-architecture.md
section 7: report, do not infer).

Outputs
  `var/reports/validation/<asset_id>.json`  the full acceptance report
  the manifest's own `validation` record       outcome + checks + report path

Exit codes: 0 pass, 3 acceptance failure (exit 3 also for a malformed manifest
treated as a validation problem; a missing/unreadable manifest is exit 2).
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .. import __version__
from ..core import Context, Status, command
from ..core.errors import EXIT_OK, EXIT_VALIDATION, ToolkitError, UsageError
from ..core.manifest import Manifest
from ..validate.checks import OUTCOME_FAIL, OUTCOME_PASS, OUTCOME_WARN, run_acceptance, semantic_hash

MANIFEST_DIR = "manifests"


@command("validate_asset", "Run acceptance checks on a manifest and its artifacts",
         stage="8 quality assurance")
def validate_asset(ctx: Context) -> Status:
    status = ctx.new_status("validate_asset")

    manifest_path = _resolve_manifest_path(ctx)
    if not manifest_path.exists():
        raise UsageError(f"manifest not found: {manifest_path}",
                         hint="run the producing stage first, or pass --out <manifest.json>")
    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise UsageError(f"manifest is not valid JSON: {manifest_path.name}: {exc}") from exc
    if not isinstance(raw, dict):
        raise UsageError(f"manifest must be a JSON object: {manifest_path}")

    workspace_root = _workspace_root(ctx)
    report = run_acceptance(raw, workspace_root=workspace_root)

    asset_id = str(raw.get("asset_id") or manifest_path.stem)
    semantic = semantic_hash(raw)
    report_payload = {
        "asset_id": asset_id,
        "generated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "manifest_path": str(manifest_path),
        "manifest_semantic_hash": semantic,
        "outcome": report.outcome,
        "counts": report.counts(),
        "checks": report.check_dicts(),
        "tool": {
            "name": "stalthos validate_asset",
            "version": __version__,
            "blender_required": False,
            "offline": True,
        },
        "seed": ctx.seed,
    }

    warnings = [f"{check.id}: {check.detail}" for check in report.checks if check.outcome == OUTCOME_WARN]
    for message in warnings:
        status.warn(message)

    status.asset_id = asset_id
    status.metrics = {
        "outcome": report.outcome,
        **report.counts(),
        "manifest_semantic_hash": semantic,
        "report_path": str(_report_path(ctx, asset_id)),
        "blender_required": False,
    }

    if ctx.options.get("dry_run"):
        status.log("dry-run: no report written, manifest not updated")
        status.exit_code = EXIT_OK if report.outcome == OUTCOME_PASS else EXIT_VALIDATION
        status.ok = report.outcome == OUTCOME_PASS
        return status

    report_path = _report_path(ctx, asset_id)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report_payload, indent=2, default=str) + "\n", encoding="utf-8")
    status.add_artifact(ctx.artifact(report_path, role="report"))
    status.log(f"wrote {report_path}")

    updated = _update_manifest(manifest_path, raw, report, report_path, ctx)
    if updated is not None:
        status.add_artifact(ctx.artifact(updated, role="report"))
        status.log(f"updated manifest {updated}")

    failed = report.counts()[OUTCOME_FAIL]
    status.exit_code = EXIT_OK if report.outcome == OUTCOME_PASS else EXIT_VALIDATION
    status.ok = report.outcome == OUTCOME_PASS
    if status.exit_code == EXIT_VALIDATION:
        status.errors.append({
            "kind": "ValidationFailed",
            "message": f"{failed} acceptance check(s) failed for {asset_id}",
            "detail": {"report_path": str(report_path),
                       "failed_checks": [c.id for c in report.checks if c.outcome == OUTCOME_FAIL]},
            "hint": f"see {report_path}",
        })
        status.partial = not report.outcome == OUTCOME_PASS
    status.provenance = {
        "generator": "stalthos-toolkit",
        "toolkit_version": __version__,
        "tool_versions": {"blender": "not-required", "python": _python_version()},
        "command": "validate_asset",
        "seed": ctx.seed,
        "input_hashes": {str(manifest_path): _sha256(manifest_path)},
    }
    return status


# --- helpers ----------------------------------------------------------------
def _resolve_manifest_path(ctx: Context) -> Path:
    explicit = ctx.options.get("out")
    if explicit:
        return Path(explicit).expanduser()
    asset_id = ctx.options.get("asset_id")
    directory = ctx.workspace.reports / MANIFEST_DIR
    if asset_id:
        return directory / f"{asset_id}.json"
    # Fall back to the definition's id: the common one-manifest-per-species case.
    definition_id = _definition_id(ctx)
    if definition_id:
        return directory / f"{definition_id}.json"
    raise UsageError(
        "no manifest selected",
        hint="pass --asset-id <id> (manifests live in var/reports/manifests/) or --out <manifest.json>",
    )


def _report_path(ctx: Context, asset_id: str) -> Path:
    return ctx.workspace.reports / "validation" / f"{asset_id}.json"


def _definition_id(ctx: Context) -> str | None:
    """Best-effort species id from -d/--definition; never fatal."""
    if not ctx.options.get("definition"):
        return None
    try:
        return str(ctx.load_definition(ctx.options.get("definition")).get("id") or "") or None
    except ToolkitError:
        return None


def _workspace_root(ctx: Context) -> Path:
    return ctx.workspace.root


def _update_manifest(manifest_path: Path, raw: dict[str, Any], report: Any,
                     report_path: Path, ctx: Context) -> Path | None:
    """Write the verdict back into the manifest, preserving unknown keys."""
    try:
        manifest = Manifest.from_dict(raw)
    except TypeError as exc:
        # A manifest with unexpected structure: the schema check already failed;
        # do not make it worse by refusing to record the verdict.
        return None
    manifest.validation.outcome = report.outcome
    manifest.validation.checks = [c.to_dict() for c in report.checks]
    manifest.validation.report_path = str(report_path)

    payload = raw  # keep keys the frozen dataclass does not model
    payload["validation"] = {
        "outcome": manifest.validation.outcome,
        "checks": manifest.validation.checks,
        "report_path": manifest.validation.report_path,
    }
    manifest_path.write_text(json.dumps(payload, indent=2, default=str) + "\n", encoding="utf-8")
    return manifest_path


def _sha256(path: Path) -> str:
    from ..core.seeding import sha256_file

    return sha256_file(path)


def _python_version() -> str:
    import sys

    return ".".join(str(p) for p in sys.version_info[:3])


__all__ = ["validate_asset", "MANIFEST_DIR"]