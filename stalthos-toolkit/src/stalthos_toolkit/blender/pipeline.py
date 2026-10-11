"""Shared plumbing for the Blender-backed commands.

Keeps the seven command modules thin and consistent: resolve Blender (exit 4
if absent), build the params payload, run headless, map the result envelope onto
the status envelope, hash every artifact, write the provenance manifest.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Iterable

from ..core.contract import Context
from ..core.errors import EXIT_EXTERNAL, ExternalToolFailed, UsageError
from ..core.manifest import ArtifactRef, Manifest, Metric
from ..core.status import Artifact, Status
from . import params as P
from .runner import BlenderRun, blender_version, resolve_blender, run_blender

#: Exit code used when Blender itself reports a structured failure.
EXTERNAL = EXIT_EXTERNAL


def require_kind(definition: dict, expected: str, command: str) -> str:
    """Refuse a definition of the wrong kind.

    The loader validates against whichever schema the file satisfies, so a
    *creature* definition fed to ``generate_plant`` loads cleanly and would
    otherwise be turned into a plausible-looking plant. That is a usage error
    (exit 2), not something to paper over with defaults.
    """
    kind = str(definition.get("kind") or "").strip().lower()
    if kind and kind != expected:
        raise UsageError(
            f"{command} expects a {expected} definition, but this one declares kind={kind!r}",
            detail={"expected_kind": expected, "actual_kind": kind,
                    "definition_id": definition.get("id")},
            hint=f"pass a {expected} definition, or use the command that owns that kind",
        )
    return kind


def require_blender(ctx: Context) -> str:
    """Resolve the Blender binary up front. Raises MissingDependency (exit 4)."""
    return resolve_blender(ctx)


def check_result(run: BlenderRun) -> dict[str, Any]:
    """Turn ``ok: false`` in the result envelope into ExternalToolFailed (6).

    core.errors.ExternalToolFailed's signature is frozen (``tool``, ``returncode``,
    ``stderr``), so the structured detail travels as JSON inside ``stderr``.
    """
    result = run.result_or_raise()
    if not result.get("ok", False):
        err = result.get("error", {}) or {}
        detail = {
            "message": err.get("message", ""),
            "kind": err.get("kind", ""),
            "action": result.get("action", "bpy"),
            "warnings": result.get("warnings", []),
            "log": str(run.log_path),
            "result_json": result,
        }
        raise ExternalToolFailed("blender", run.returncode,
                                 stderr=json.dumps(detail, indent=2, default=str))
    return result


def prepare_outputs(ctx: Context, relative_paths: Iterable[str]) -> dict[str, str]:
    """Create output directories under the workspace and return absolute paths."""
    out: dict[str, str] = {}
    for rel in relative_paths:
        path = ctx.workspace.path_for(rel)
        path.parent.mkdir(parents=True, exist_ok=True)
        out[rel] = str(path)
    return out


def add_artifacts(ctx: Context, status: Status, entries: Iterable[dict[str, Any]],
                  *, tag: str = "") -> list[str]:
    """Hash every file Blender produced and register it on the status."""
    paths: list[str] = []
    for entry in entries:
        path = Path(entry["path"])
        if not path.exists():
            status.warn(f"{tag or 'blender'} reported {path} but the file is missing")
            continue
        artifact = ctx.artifact(path, role=entry.get("role", "output"))
        status.add_artifact(artifact)
        paths.append(str(path))
        if entry.get("warnings"):
            status.warnings.extend(str(w) for w in entry["warnings"])
    return paths


def result_warnings(status: Status, result: dict[str, Any]) -> None:
    for warning in result.get("warnings", []):
        status.warn(f"[{result.get('action', 'bpy')}] {warning}")


def record_metrics(status: Status, result: dict[str, Any], *, extra: dict[str, Any] | None = None) -> dict[str, Any]:
    metrics = dict(result.get("metrics", {}))
    metrics["blender"] = result.get("blender_version", "unknown")
    metrics["engine"] = result.get("engine")
    metrics["timings_s"] = result.get("timings", {})
    metrics["bpy_total_s"] = result.get("total_s")
    if extra:
        metrics.update(extra)
    status.metrics.update(metrics)
    return metrics


def write_manifest(
    ctx: Context,
    status: Status,
    *,
    asset_id: str,
    kind: str,
    command: str,
    artifacts: list[str],
    metrics: dict[str, Any],
    result: dict[str, Any],
    extra_artifacts: list[ArtifactRef] | None = None,
    parent_asset_id: str | None = None,
    notes: str = "",
) -> Manifest:
    """One provenance manifest per produced asset, under reports/manifests/."""
    manifest = ctx.new_manifest(asset_id=asset_id, kind=kind, command=command)
    manifest.provenance.tool_versions = {"blender": result.get("blender_version", "unknown")}
    manifest.provenance.parent_asset_id = parent_asset_id
    manifest.notes = notes
    roles = {"source": "source", "intermediate": "intermediate", "runtime": "runtime",
             "preview": "preview", "output": "output"}
    for path in artifacts:
        ref = ArtifactRef(path=path, role=_role_for(path, ctx), sha256=_sha(path), bytes=_size(path))
        manifest.artifacts.append(ref)
    for ref in extra_artifacts or []:
        manifest.artifacts.append(ref)

    object_rows = metrics.get("objects") or []
    if object_rows:
        manifest.add_metric("triangle_count", float(sum(o.get("triangles", 0) for o in object_rows)),
                             "triangles", measured=True)
        manifest.add_metric("vertex_count", float(sum(o.get("vertices", 0) for o in object_rows)),
                             "vertices", measured=True)
    for key, unit in (("bone_count", "bones"), ("trunk_segments", "segments"),
                      ("neck_segments", "segments"), ("tail_segments", "segments"),
                      ("pinnae", "pinnae"), ("branch_count", "branches"),
                      ("stem_count", "stems"), ("leaf_count", "leaves"),
                      ("frond_count", "fronds"), ("renders", "images"),
                      ("texture_count", "textures"), ("resolution", "px")):
        value = metrics.get(key)
        if isinstance(value, (int, float)):
            manifest.add_metric(key, float(value), unit, measured=True)
    for key in ("height_m", "total_length_m", "stance_height_m"):
        value = metrics.get(key)
        if isinstance(value, (int, float)):
            manifest.add_metric(key, float(value), "m", measured=True)
    previews = [a.path for a in manifest.artifacts if a.role == "preview"]
    manifest.previews = previews
    path = ctx.workspace.reports / "manifests" / f"{asset_id}.json"
    manifest.save(path)
    status.add_artifact(ctx.artifact(path, role="report"))
    return manifest


def _role_for(path: str, ctx: Context) -> str:
    p = Path(path).resolve()
    root = ctx.workspace.root
    for role, folder in (("source", "sources"), ("intermediate", "intermediate"),
                         ("runtime", "runtime"), ("preview", "preview")):
        if str(p).startswith(str((root / folder).resolve())):
            return role
    return "output"


def _sha(path: str) -> str:
    from ..core.seeding import sha256_file

    try:
        return sha256_file(path)
    except OSError:
        return ""


def _size(path: str) -> int:
    try:
        return Path(path).stat().st_size
    except OSError:
        return 0


def base_params(
    ctx: Context,
    *,
    command: str,
    asset_id: str,
    definition: dict[str, Any],
    stage: str | None,
    variant: str | None,
    outputs: dict[str, str],
    engine: str = "EEVEE",
    resolution: int = 512,
) -> dict[str, Any]:
    """The params envelope every bpy script receives."""
    return {
        "tool": "stalthos-toolkit",
        "command": command,
        "asset_id": asset_id,
        "seed": ctx.seed,
        "stage": stage,
        "variant": variant,
        "definition_id": definition.get("id") or definition.get("species_id"),
        "definition": definition,
        "engine": engine,
        "resolution": int(resolution),
        "outputs": outputs,
        "strict": bool(ctx.options.get("strict")),
    }


def run_script(
    ctx: Context,
    status: Status,
    *,
    script: str,
    params: dict[str, Any],
    log_name: str,
) -> tuple[BlenderRun, dict[str, Any]]:
    """Run one bpy script, map failures onto typed errors, record warnings."""
    run = run_blender(ctx, script=script, params=params, log_name=log_name)
    result = check_result(run)
    result_warnings(status, result)
    if run.log_path is not None and run.log_path.exists():
        status.add_artifact(ctx.artifact(run.log_path, role="report"))
    status.metrics["blender_run"] = run.to_dict()
    status.metrics["tool_versions"] = {"blender": result.get("blender_version", blender_version(ctx))}
    return run, result


def tool_versions(ctx: Context) -> dict[str, str]:
    return {"blender": blender_version(ctx)}


__all__ = [
    "P",
    "add_artifacts",
    "base_params",
    "check_result",
    "prepare_outputs",
    "record_metrics",
    "require_blender",
    "require_kind",
    "result_warnings",
    "run_script",
    "tool_versions",
    "write_manifest",
]
