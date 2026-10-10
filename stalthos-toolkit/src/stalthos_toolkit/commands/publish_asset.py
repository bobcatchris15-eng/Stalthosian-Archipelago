"""`publish_asset` -- canonical pipeline stage 9 (publish).

Promotes the optimized artifacts of one asset from `var/intermediate/` into
`var/runtime/<asset_id>/` and writes a build receipt to
`var/reports/receipts/<asset_id>.json`.

What blocks publication (decided -- do not relitigate)
-----------------------------------------------------
Only **structural** failures block, and each one exits 3:

* `manifest.validation.outcome != "pass"` (including `not_run`),
* any recorded upstream status with `partial: true`,
* a declared artifact that is missing on disk or whose sha256 does not match,
* incomplete provenance: no integer seed, no input hashes, or no tool versions.

What does NOT block
-------------------
* `references[].usage_rights` of `unknown` / `unverified`: this is a personal,
  non-commercial project, so an unestablished usage right is **warned about and
  recorded in the receipt**, never a hard stop.
* an uncompressed texture or a degraded LOD chain: warned about, recorded in the
  receipt with `measured=False`, and flagged so nothing downstream mistakes it
  for an optimized asset.

Engine-specific deployment is out of scope: no engine has been chosen, so the
publish target is `var/runtime/`.
"""

from __future__ import annotations

import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .. import __version__
from ..core import Context, Status, command
from ..core.errors import EXIT_OK, EXIT_VALIDATION, ExternalToolFailed
from ..core.seeding import sha256_file
from ..optimize.pipeline_state import (
    allow_degraded,
    asset_id_from,
    load_manifest,
    resolve,
    resolve_manifest_path,
    semantic_content_hash,
    update_manifest,
)
from ..optimize.tools import discover_many

#: artifact roles that are promoted into `var/runtime/`.
PROMOTED_ROLES = ("intermediate", "runtime", "texture", "collision", "rig", "animation")

#: roles that stay where they are (masters and review material).
RETAINED_ROLES = ("source", "preview", "report")

UNVERIFIED_RIGHTS = ("unknown", "unverified", "unestablished", "", "none", "tbd")


@command("publish_asset", "Promote validated artifacts to var/runtime with a build receipt",
         stage="9 publish")
def publish_asset(ctx: Context) -> Status:
    status = ctx.new_status("publish_asset")
    if ctx.options.get("definition"):
        ctx.load_definition(ctx.options.get("definition"))

    manifest_path = resolve_manifest_path(ctx)
    manifest = load_manifest(manifest_path)
    asset_id = asset_id_from(ctx, manifest)
    status.asset_id = asset_id

    blockers: list[dict[str, Any]] = []
    warnings: list[str] = []

    blockers += _check_validation(manifest)
    blockers += _check_partial(ctx, asset_id)
    blockers += _check_provenance(manifest)
    artifact_report = _check_artifacts(manifest, ctx.workspace.root)
    blockers += artifact_report["blockers"]

    warnings += _licensing_warnings(manifest)
    warnings += _degradation_warnings(manifest)

    if blockers:
        for entry in blockers:
            status.warn(f"publish blocked: {entry['message']}")
        status.partial = True
        status.metrics = {
            "published": False,
            "blockers": blockers,
            "warnings": warnings,
            "content_hash": semantic_content_hash(manifest),
        }
        status.ok = False
        status.exit_code = EXIT_VALIDATION
        status.errors.append({
            "kind": "PublishBlocked",
            "message": f"{len(blockers)} structural problem(s) block publication of {asset_id}",
            "detail": {"blockers": blockers, "warnings": warnings},
            "hint": "fix the failing stage and re-run; see var/reports/manifests/<asset_id>.json",
        })
        status.provenance = _provenance(ctx, warnings)
        return status

    runtime_dir = ctx.workspace.runtime / asset_id
    promotions = _plan_promotions(manifest, ctx.workspace.root, runtime_dir)
    collisions = _promotion_collisions(promotions)
    if collisions:
        blockers += collisions
        for entry in collisions:
            status.warn(f"publish blocked: {entry['message']}")
        status.partial = True
        status.metrics = {
            "published": False,
            "blockers": blockers,
            "warnings": warnings,
            "content_hash": semantic_content_hash(manifest),
        }
        status.ok = False
        status.exit_code = EXIT_VALIDATION
        status.errors.append({
            "kind": "PublishBlocked",
            "message": f"{len(blockers)} structural problem(s) block publication of {asset_id}",
            "detail": {"blockers": blockers, "warnings": warnings},
            "hint": "two manifest artifacts would overwrite each other in var/runtime/; "
                    "rename or re-register one of them in the manifest",
        })
        status.provenance = _provenance(ctx, warnings)
        return status

    status.metrics = {
        "published": False,
        "runtime_dir": str(runtime_dir),
        "promotions": [str(entry["target"]) for entry in promotions],
        "content_hash": semantic_content_hash(manifest),
        "warnings": warnings,
        "licensing": _licensing_summary(manifest),
    }

    if ctx.options.get("dry_run"):
        status.log("dry-run: nothing promoted, no receipt written")
        status.provenance = _provenance(ctx, warnings)
        return status

    copied, errors = _copy_all(promotions, runtime_dir, ctx)
    if errors:
        raise ExternalToolFailed("publish_asset.copy", 1, stderr=json.dumps(errors, default=str))

    published_records = _publish_manifest(manifest, promotions, ctx, warnings, asset_id)

    receipt = _receipt(manifest, published_records, ctx, asset_id, warnings, content_hash=None)
    receipt_path = ctx.workspace.reports / "receipts" / f"{asset_id}.json"
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    receipt_path.write_text(json.dumps(receipt, indent=2, default=str) + "\n", encoding="utf-8")

    for artifact in copied:
        status.add_artifact(artifact)
    status.add_artifact(ctx.artifact(receipt_path, role="report"))
    status.add_artifact(ctx.artifact(ctx.workspace.reports / "manifests" / f"{asset_id}.json",
                                     role="report"))
    status.add_artifact(ctx.artifact(runtime_dir / "manifest.json", role="report"))

    for message in warnings:
        status.warn(message)

    status.metrics["published"] = True
    status.metrics["receipt_path"] = str(receipt_path)
    status.metrics["content_hash"] = receipt["content_hash"]
    status.metrics["artifacts_published"] = len(published_records)
    status.provenance = _provenance(ctx, warnings, receipt=receipt)
    status.exit_code = EXIT_OK
    status.ok = True
    return status


# --- gates ------------------------------------------------------------------
def _check_validation(manifest: dict[str, Any]) -> list[dict[str, Any]]:
    outcome = str((manifest.get("validation") or {}).get("outcome", "not_run"))
    if outcome == "pass":
        return []
    return [{"kind": "ValidationNotPassed", "message": f"validation outcome is {outcome!r}, expected 'pass'"}]


def _check_partial(ctx: Context, asset_id: str) -> list[dict[str, Any]]:
    """Refuse to promote anything an upstream stage marked partial."""
    if bool(manifest_partial_flag(ctx, asset_id)):
        return [{"kind": "PartialUpstream",
                 "message": f"an upstream status for {asset_id} is marked partial"}]
    return []


def manifest_partial_flag(ctx: Context, asset_id: str) -> bool:
    status_dir = ctx.workspace.reports / "status"
    if not status_dir.exists():
        return False
    for path in sorted(status_dir.glob(f"*_{asset_id}_*.json")):
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if isinstance(payload, dict) and payload.get("partial") is True:
            return True
    pipeline = status_dir / f"pipeline_{asset_id}.json"
    if pipeline.exists():
        try:
            payload = json.loads(pipeline.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return False
        for entry in (payload.get("stages") or payload.get("statuses") or []):
            if isinstance(entry, dict) and entry.get("partial") is True:
                return True
    return False


def _check_provenance(manifest: dict[str, Any]) -> list[dict[str, Any]]:
    provenance = manifest.get("provenance") or {}
    blockers: list[dict[str, Any]] = []
    seed = provenance.get("seed")
    if not isinstance(seed, int) or isinstance(seed, bool):
        blockers.append({"kind": "ProvenanceSeedMissing",
                         "message": "provenance.seed is missing or not an integer"})
    hashes = provenance.get("input_hashes") or {}
    if not hashes:
        blockers.append({"kind": "ProvenanceInputHashesMissing",
                         "message": "provenance.input_hashes is empty; the run cannot be reproduced"})
    elif any(not isinstance(v, str) or len(v) != 64 for v in hashes.values()):
        blockers.append({"kind": "ProvenanceInputHashesInvalid",
                         "message": "provenance.input_hashes holds a value that is not a sha256"})
    if not (provenance.get("tool_versions") or {}):
        blockers.append({"kind": "ProvenanceToolVersionsMissing",
                         "message": "provenance.tool_versions is empty"})
    return blockers


def _check_artifacts(manifest: dict[str, Any], root: Path) -> dict[str, Any]:
    blockers: list[dict[str, Any]] = []
    checked = 0
    for artifact in manifest.get("artifacts") or []:
        if not isinstance(artifact, dict):
            continue
        role = str(artifact.get("role", ""))
        if role in RETAINED_ROLES:
            continue
        checked += 1
        path = resolve(str(artifact.get("path", "")), root)
        if not path.exists():
            blockers.append({"kind": "ArtifactMissing",
                             "message": f"artifact {artifact.get('path')} does not exist",
                             "detail": {"path": str(path)}})
            continue
        declared = artifact.get("sha256")
        if not declared:
            blockers.append({"kind": "ArtifactUnhashed",
                             "message": f"artifact {artifact.get('path')} carries no sha256"})
            continue
        actual = sha256_file(path)
        if actual != declared:
            blockers.append({"kind": "ArtifactHashMismatch",
                             "message": f"artifact {artifact.get('path')} hash {actual[:12]}... "
                                        f"!= recorded {str(declared)[:12]}...",
                             "detail": {"path": str(path), "actual": actual, "recorded": declared}})
    return {"blockers": blockers, "checked": checked}


def _licensing_summary(manifest: dict[str, Any]) -> dict[str, Any]:
    """Machine-readable licensing record. Never a gate -- see i-0002."""
    provenance = manifest.get("provenance") or {}
    unverified: list[str] = []
    for reference in provenance.get("references") or []:
        if not isinstance(reference, dict):
            continue
        if str(reference.get("usage_rights", "unknown")).lower() in UNVERIFIED_RIGHTS:
            unverified.append(str(reference.get("id", "<unnamed>")))
    return {
        "licenses": provenance.get("licenses", []),
        "references": provenance.get("references", []),
        "unverified_usage_rights": sorted(unverified),
        "usage_rights_block_publication": False,
        "policy": "unverified usage_rights warn and are recorded; they do not block "
                  "publication of this personal, non-commercial project (intent i-0002)",
    }


def _licensing_warnings(manifest: dict[str, Any]) -> list[str]:
    """Unestablished usage rights warn and are carried into the receipt.

    Per intent i-0002 this NEVER blocks: a personal, non-commercial project
    records provenance and ships. Only structural failures block.
    """
    warnings: list[str] = []
    provenance = manifest.get("provenance") or {}
    unverified: list[str] = []
    for reference in provenance.get("references") or []:
        if not isinstance(reference, dict):
            continue
        if str(reference.get("usage_rights", "unknown")).lower() in UNVERIFIED_RIGHTS:
            unverified.append(str(reference.get("id", "<unnamed>")))
    if unverified:
        warnings.append(
            f"{len(unverified)} reference(s) have unestablished usage rights "
            f"({', '.join(sorted(unverified))}); publication is allowed (personal, "
            "non-commercial project) and the warning is recorded in the receipt")
    if not (provenance.get("licenses") or []):
        warnings.append("no license provenance recorded on the manifest; recorded in the receipt")
    return warnings


def _degradation_warnings(manifest: dict[str, Any]) -> list[str]:
    warnings: list[str] = []
    for artifact in manifest.get("artifacts") or []:
        settings = (artifact.get("importer_settings") or {}) if isinstance(artifact, dict) else {}
        if settings.get("degraded"):
            warnings.append(
                f"artifact {artifact.get('path')} was produced by a degraded path "
                f"(simplifier/compression: {settings.get('simplifier', 'degraded')}); "
                "its metrics are recorded with measured=False")
    return warnings


# --- promotion --------------------------------------------------------------
def _plan_promotions(manifest: dict[str, Any], root: Path, runtime_dir: Path) -> list[dict[str, Any]]:
    plans: list[dict[str, Any]] = []
    for artifact in manifest.get("artifacts") or []:
        if not isinstance(artifact, dict):
            continue
        role = str(artifact.get("role", ""))
        if role not in PROMOTED_ROLES:
            continue
        source = resolve(str(artifact.get("path", "")), root)
        try:
            relative = source.relative_to(root)
        except ValueError:
            relative = Path(source.name)
        # Flatten the intermediate/<kind>/<asset_id>/ prefix into the runtime dir.
        parts = list(relative.parts)
        if asset_id_in(parts):
            parts = parts[parts.index(asset_id_in(parts)) + 1:]
        target = runtime_dir.joinpath(*parts) if parts else runtime_dir / source.name
        plans.append({
            "artifact": artifact,
            "source": source,
            "target": target,
            "role": role,
            "name": source.name,
        })
    return plans


def _promotion_collisions(plans: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Two sources that flatten onto one runtime path.

    Copying them would silently overwrite: the receipt would then list more
    artifacts than files exist in var/runtime/. That is a structural problem,
    not a cosmetic one, so it blocks.
    """
    by_target: dict[str, list[str]] = {}
    for plan in plans:
        by_target.setdefault(str(plan["target"]), []).append(str(plan["source"]))
    return [
        {"kind": "PromotionCollision",
         "message": f"{len(sources)} source artifacts flatten onto the same runtime path {target}",
         "detail": {"target": target, "sources": sorted(sources)}}
        for target, sources in sorted(by_target.items()) if len(sources) > 1
    ]


def asset_id_in(parts: list[str]) -> str | None:
    for part in parts:
        if part.startswith(("plant.", "creature.", "prop.", "fungus.")):
            return part
    return None


def _copy_all(plans: list[dict[str, Any]], runtime_dir: Path, ctx: Context) -> tuple[list[Any], list[dict]]:
    copied: list[Any] = []
    errors: list[dict] = []
    runtime_dir.mkdir(parents=True, exist_ok=True)
    for plan in plans:
        source: Path = plan["source"]
        target: Path = plan["target"]
        try:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        except OSError as exc:
            errors.append({"path": str(source), "error": str(exc)})
            continue
        copied.append(ctx.artifact(target, role="runtime"))
    return copied, errors


def _publish_manifest(manifest: dict[str, Any], plans: list[dict[str, Any]], ctx: Context,
                      warnings: list[str], asset_id: str) -> list[dict[str, Any]]:
    """Rewrite artifact records to their published role/path and save twice."""
    artifacts = [dict(a) for a in manifest.get("artifacts") or [] if isinstance(a, dict)]
    by_path = {str(a.get("path")): a for a in artifacts}
    published: list[dict[str, Any]] = []

    for plan in plans:
        record = by_path.get(str(plan["artifact"].get("path")))
        if record is None:
            continue
        record["role"] = "runtime"
        record["path"] = str(plan["target"])
        record["sha256"] = sha256_file(plan["target"])
        record["bytes"] = plan["target"].stat().st_size
        record.setdefault("importer_settings", {})
        published.append(record)

    manifest = dict(manifest)
    manifest["artifacts"] = artifacts
    notes = manifest.get("notes")
    manifest["notes"] = ((notes + "\n") if notes else "") + \
        f"published {len(published)} artifact(s); licensing warnings: {len(warnings)}"

    manifest_path = update_manifest(ctx.workspace.reports / "manifests" /
                                    f"{str(manifest.get('asset_id') or asset_id)}.json", manifest)
    runtime_copy = ctx.workspace.runtime / asset_id / "manifest.json"
    runtime_copy.parent.mkdir(parents=True, exist_ok=True)
    runtime_copy.write_text(manifest_path.read_text(encoding="utf-8"), encoding="utf-8")
    return published


def _receipt(manifest: dict[str, Any], published: list[dict[str, Any]], ctx: Context,
             asset_id: str, warnings: list[str], *, content_hash: str | None = None) -> dict[str, Any]:
    provenance = manifest.get("provenance") or {}
    return {
        "schema_version": "1.0",
        "asset_id": asset_id,
        "kind": manifest.get("kind"),
        "stage": manifest.get("stage"),
        "variant": manifest.get("variant"),
        "published_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "content_hash": content_hash or semantic_content_hash(manifest),
        "seed": provenance.get("seed"),
        "tool_versions": dict(provenance.get("tool_versions") or {}),
        "publish_tool_versions": {"stalthos-toolkit": __version__},
        "input_hashes": dict(provenance.get("input_hashes") or {}),
        "artifacts": [
            {"path": entry.get("path"), "role": entry.get("role"), "sha256": entry.get("sha256"),
             "bytes": entry.get("bytes"),
             "importer_settings": entry.get("importer_settings", {})}
            for entry in published
        ],
        "licensing": {
            **_licensing_summary(manifest),
            "warnings": [w for w in warnings if "usage rights" in w or "license" in w],
        },
        "degradation": {
            "allow_degraded": allow_degraded(ctx)[0],
            "warnings": [w for w in warnings if "degraded" in w],
        },
        "validation": manifest.get("validation", {}).get("outcome"),
        "notes": manifest.get("notes", ""),
    }


def _provenance(ctx: Context, warnings: list[str], receipt: dict[str, Any] | None = None) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "generator": "stalthos-toolkit",
        "toolkit_version": __version__,
        "tool_versions": {name: state.version or ("present" if state.available else "not-available")
                          for name, state in discover_many(["gltfpack", "compressonator"]).items()},
        "command": "publish_asset",
        "seed": ctx.seed,
        "input_hashes": ctx.input_hashes(),
        "warnings": warnings,
    }
    if receipt:
        payload["receipt_content_hash"] = receipt["content_hash"]
    return payload


__all__ = ["PROMOTED_ROLES", "publish_asset"]