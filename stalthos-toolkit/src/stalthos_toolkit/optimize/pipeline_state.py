"""Shared helpers for the optimize/publish commands.

Resolving a manifest, an asset id, a source asset and the degraded-mode flag is
identical for `generate_lods`, `benchmark_asset` and `publish_asset`, so it lives
here rather than being copy-pasted three times.

`allow_degraded`
----------------
The CLI in `cli.py` is frozen and does not yet declare `--allow-degraded`, so
the flag is read, in order, from:

1. ``ctx.options["allow_degraded"]`` (once worker zero wires the flag),
2. the ``STEALTHOS_ALLOW_DEGRADED=1`` environment variable,
3. otherwise False — i.e. the strict default.

That keeps the decided behaviour (Compressonator absent -> exit 4 by default)
true today, without pretending the flag exists on the command line.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any

from ..core.contract import Context
from ..core.errors import UsageError

MANIFEST_DIR = "manifests"

GEOMETRY_SUFFIXES = (".glb", ".gltf")
BLEND_SUFFIXES = (".blend",)
TEXTURE_SUFFIXES = (".png", ".tga", ".dds", ".exr", ".ktx2", ".jpg", ".jpeg", ".webp")


def allow_degraded(ctx: Context) -> tuple[bool, str]:
    """(enabled, where the value came from). Defaults to False."""
    value = ctx.options.get("allow_degraded")
    if value is not None:
        return bool(value), "option"
    env = os.environ.get("STEALTHOS_ALLOW_DEGRADED")
    if env is not None and env.strip().lower() in ("1", "true", "yes", "on"):
        return True, "STEALTHOS_ALLOW_DEGRADED"
    return False, "default"


def load_manifest(path: str | Path) -> dict[str, Any]:
    path = Path(path)
    if not path.exists():
        raise UsageError(f"manifest not found: {path}",
                         hint="run the stage that produces the asset first, or pass --out <manifest.json>")
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise UsageError(f"manifest is not valid JSON: {path.name}: {exc}") from exc
    if not isinstance(raw, dict):
        raise UsageError(f"manifest must be a JSON object: {path}")
    return raw


def resolve_manifest_path(ctx: Context) -> Path:
    """`--out` wins, then `--asset-id`, then the definition's id."""
    explicit = ctx.options.get("out")
    if explicit:
        return Path(explicit).expanduser()
    directory = ctx.workspace.reports / MANIFEST_DIR
    asset_id = ctx.options.get("asset_id")
    if asset_id:
        return directory / f"{asset_id}.json"
    definition_id = _definition_id(ctx)
    if definition_id:
        return directory / f"{definition_id}.json"
    raise UsageError(
        "no manifest selected",
        hint="pass --asset-id <id> (manifests live in var/reports/manifests/) or --out <manifest.json>")


def _definition_id(ctx: Context) -> str | None:
    if not ctx.options.get("definition"):
        return None
    try:
        return str(ctx.load_definition(ctx.options.get("definition")).get("id") or "") or None
    except Exception:
        return None


def asset_id_from(ctx: Context, manifest: dict[str, Any]) -> str:
    for candidate in (manifest.get("asset_id"), ctx.options.get("asset_id"), ctx.options.get("variant")):
        if candidate:
            return str(candidate)
    raise UsageError("cannot determine asset_id", hint="pass --asset-id <id>")


def resolve(path_text: str, workspace_root: Path) -> Path:
    path = Path(path_text)
    if not path.is_absolute():
        return (workspace_root / path).resolve()
    return path


def artifacts(manifest: dict[str, Any]) -> list[dict[str, Any]]:
    return [a for a in (manifest.get("artifacts") or []) if isinstance(a, dict)]


def metrics_map(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {str(m.get("name")): m for m in (manifest.get("metrics") or []) if isinstance(m, dict)}


def find_source_asset(manifest: dict[str, Any], workspace_root: Path) -> dict[str, Any] | None:
    """The source geometry artifact a stage should build from.

    Preference: an interchange asset (.glb/.gltf) that already exists, then a
    Blender master. Returns the artifact dict (with a resolved `path`), or None.
    """
    entries = artifacts(manifest)
    for suffix in GEOMETRY_SUFFIXES:
        for entry in entries:
            if str(entry.get("path", "")).lower().endswith(suffix):
                resolved = dict(entry)
                resolved["_resolved"] = str(resolve(str(entry.get("path", "")), workspace_root))
                return resolved
    for suffix in BLEND_SUFFIXES:
        for entry in entries:
            if str(entry.get("path", "")).lower().endswith(suffix):
                resolved = dict(entry)
                resolved["_resolved"] = str(resolve(str(entry.get("path", "")), workspace_root))
                return resolved
    return None


def texture_slots(manifest: dict[str, Any], workspace_root: Path) -> list[tuple[Path, str]]:
    """(path, slot kind) pairs for every declared texture artifact."""
    out: list[tuple[Path, str]] = []
    for entry in artifacts(manifest):
        path_text = str(entry.get("path", ""))
        if not path_text.lower().endswith(TEXTURE_SUFFIXES):
            continue
        settings = entry.get("importer_settings") or {}
        kind = str(settings.get("kind") or "unknown")
        out.append((resolve(path_text, workspace_root), kind))
    return out


def update_manifest(path: str | Path, raw: dict[str, Any], *,
                    metrics: list[dict[str, Any]] | None = None,
                    artifacts_to_add: list[dict[str, Any]] | None = None,
                    validation: dict[str, Any] | None = None,
                    notes: str | None = None) -> Path:
    """Merge new records into a manifest dict and write it back.

    Metrics are replaced by name (a re-run of the stage does not accumulate
    duplicates); artifacts are replaced by path.
    """
    payload = json.loads(json.dumps(raw, default=str))  # defensive deep copy

    if metrics:
        existing = {str(m.get("name")): m for m in (payload.get("metrics") or []) if isinstance(m, dict)}
        for metric in metrics:
            entry = {"name": str(metric["name"]), "value": float(metric["value"]),
                     "unit": str(metric.get("unit", "")), "measured": bool(metric.get("measured", True))}
            existing[entry["name"]] = entry
        payload["metrics"] = [existing[name] for name in sorted(existing)]

    if artifacts_to_add:
        existing = {str(a.get("path")): a for a in (payload.get("artifacts") or []) if isinstance(a, dict)}
        for artifact in artifacts_to_add:
            existing[str(artifact["path"])] = artifact
        payload["artifacts"] = [existing[key] for key in sorted(existing)]

    if validation is not None:
        payload["validation"] = validation
    if notes is not None:
        payload["notes"] = notes

    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, default=str) + "\n", encoding="utf-8")
    return path


def semantic_content_hash(manifest: dict[str, Any]) -> str:
    """The hash publish receipts are written against.

    `validate/checks.py` already defines the cross-stage semantic hash (it drops
    `created_at`, `tool_versions` and the validation record); reuse it so a
    receipt and a validation report agree. Falls back to
    `core.manifest.Manifest.content_hash` if validate/ is unavailable.
    """
    try:
        from ..validate.checks import semantic_hash

        return semantic_hash(manifest)
    except Exception:  # pragma: no cover - defensive
        from ..core.manifest import Manifest

        return Manifest.from_dict(manifest).content_hash()


__all__ = [
    "BLEND_SUFFIXES",
    "GEOMETRY_SUFFIXES",
    "MANIFEST_DIR",
    "TEXTURE_SUFFIXES",
    "allow_degraded",
    "artifact_entry",
    "artifacts",
    "asset_id_from",
    "find_source_asset",
    "load_manifest",
    "metrics_map",
    "resolve",
    "resolve_manifest_path",
    "semantic_content_hash",
    "texture_slots",
    "update_manifest",
]


def artifact_entry(path: str | Path, sha256: str, size: int, role: str,
                   importer_settings: dict[str, Any] | None = None) -> dict[str, Any]:
    """An artifact record in the shape `asset-manifest.schema.json` demands."""
    entry: dict[str, Any] = {"path": str(path), "role": role, "sha256": sha256, "bytes": int(size)}
    if importer_settings:
        entry["importer_settings"] = importer_settings
    return entry