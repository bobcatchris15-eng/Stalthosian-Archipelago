"""`bake_materials` -- multiscale structure bake (pipeline stage 4).

Bakes albedo / roughness / normal / thickness from the procedural material
setup. **No directional lighting is baked into albedo**: every non-normal pass
bakes a flat, emission-only copy of the surface structure with all lights
removed and a black world, so lighting physically cannot enter the image.
"""

from __future__ import annotations

import json
from pathlib import Path

from ..blender import pipeline as PL
from ..blender.params import creature_surface_params
from ..core.contract import Context, command
from ..core.errors import UsageError
from ..core.status import Status

PASSES = ("albedo", "roughness", "normal", "thickness")


@command("bake_materials",
         "Bake multiscale material structure (albedo/roughness/normal/thickness)",
         stage="4 texture/material creation")
def bake_materials(ctx: Context) -> Status:
    status = ctx.new_status("bake_materials")
    definition = ctx.load_definition(ctx.options.get("definition"))
    # A malformed definition is a usage error (exit 2), reported before the
    # external-tool check (exit 4); --dry-run never needs Blender at all.
    asset_id = ctx.options.get("asset_id") or definition.get("id") or "asset.unnamed"
    kind = str(definition.get("kind") or ("creature" if "body_plan" in definition else "plant"))
    stage = ctx.options.get("stage")
    suffix = f".{stage}" if stage else ""

    PL.require_blender(ctx)      # exit 4: no Blender means no run
    source = _resolve_source(ctx, kind, asset_id, suffix)
    textures = ctx.workspace.sub("intermediate", "bakes", asset_id)
    textures_rel = str(textures.relative_to(ctx.workspace.root))
    blended_rel = str((ctx.workspace.intermediate / "bakes" /
                       f"{asset_id}{suffix}.baked.blend").relative_to(ctx.workspace.root))
    result_rel = str((textures / f"{asset_id}{suffix}.bake.result.json").relative_to(ctx.workspace.root))

    outputs = PL.prepare_outputs(ctx, [textures_rel, blended_rel, result_rel])
    outputs["textures"] = outputs[textures_rel]
    outputs["blend"] = outputs[blended_rel]

    resolution = int(ctx.options.get("resolution") or definition.get("texture_resolution") or 512)
    params = PL.base_params(ctx, command="bake_materials", asset_id=asset_id,
                            definition=definition, stage=stage,
                            variant=ctx.options.get("variant"), outputs=outputs,
                            resolution=resolution)
    params["source_blend"] = str(source)
    params["seed"] = ctx.seed_for("bake_materials", asset_id, stage or "adult")
    params["passes"] = list(PASSES)
    params["resolution"] = resolution
    params["margin"] = int(definition.get("bake_margin", 6))
    params["bake_samples"] = int(definition.get("bake_samples", 16))
    params["structure_preset"] = str(definition.get("bake_preset", "skin" if kind == "creature" else "bark"))
    params["scale_hint"] = float(definition.get("bake_scale_hint", 1.0))
    params["leaf_thickness_m"] = float(creature_surface_params(definition).get("scale_size_m", 0.02)) / 4.0

    if ctx.options.get("dry_run"):
        status.asset_id = asset_id
        status.metrics["plan"] = {"outputs": outputs, "passes": PASSES,
                                  "resolution": resolution, "source": str(source)}
        return status


    _, result = PL.run_script(ctx, status, script="bake", params=params,
                              log_name=f"bake_materials_{asset_id}{suffix}")

    result_file = Path(outputs[result_rel])
    result_file.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
    written = list(result.get("artifacts", [])) + [
        {"path": str(result_file), "role": "intermediate", "kind": "result_json"}
    ]
    metrics = PL.record_metrics(status, result,
                                extra={"passes": list(PASSES), "resolution": resolution})
    # the Artifact dataclass cannot carry the per-image pass label, so expose the
    # pass -> paths map explicitly; it is what validate_asset and tests read
    texture_files: dict[str, list[str]] = {}
    for entry in written:
        if entry.get("kind") == "texture":
            texture_files.setdefault(str(entry.get("pass")), []).append(str(entry["path"]))
    status.metrics["texture_files"] = texture_files
    metrics["texture_files"] = texture_files
    paths = PL.add_artifacts(ctx, status, written, tag="bake_materials")

    if metrics.get("directional_light_baked"):
        status.warn("bake reported directional light in albedo -- this violates the plan")

    PL.write_manifest(
        ctx, status, asset_id=f"{asset_id}.bake", kind="texture_set",
        command="bake_materials", artifacts=paths, metrics=metrics, result=result,
        parent_asset_id=asset_id,
        notes=("structure-only bake: albedo/roughness/thickness come from flat emission "
               "shaders with all lights removed and a black world, so no directional "
               "lighting is baked into albedo"),
    )
    # Register the verified derived master on an existing parent asset. A later
    # LOD stage must use that explicit artifact, never guess a stale bake path.
    parent_path = ctx.workspace.reports / "manifests" / f"{asset_id}.json"
    baked_master = Path(outputs[blended_rel])
    if metrics.get("material_export_ready") and parent_path.is_file() and baked_master.is_file():
        from ..optimize.pipeline_state import artifact_entry, load_manifest, update_manifest
        from ..core.seeding import sha256_file
        ref = artifact_entry(str(baked_master), sha256_file(baked_master),
                             baked_master.stat().st_size, "source",
                             {"generated_by": "bake_materials", "source_sha256": sha256_file(source),
                              "seed": ctx.seed})
        update_manifest(parent_path, load_manifest(parent_path), artifacts_to_add=[ref])
        status.add_artifact(ctx.artifact(parent_path, role="intermediate"))
    status.asset_id = f"{asset_id}.bake"
    return status


def _resolve_source(ctx: Context, kind: str, asset_id: str, suffix: str) -> Path:
    """Find the .blend master to bake: --out, the standard source path, or a glob."""
    explicit = ctx.options.get("out")
    if explicit:
        candidate = Path(explicit).expanduser()
        if not candidate.is_file():
            raise UsageError(f"--out does not point at a file: {candidate}")
        return candidate.resolve()
    sources = ctx.workspace.sources / asset_id
    preferred = sources / f"{asset_id}{suffix}.blend"
    if preferred.is_file():
        return preferred
    candidates = sorted(sources.glob("*.blend"))
    candidates = [c for c in candidates if ".rig." not in c.name and ".baked." not in c.name]
    if candidates:
        return candidates[0]
    raise UsageError(
        f"no source .blend found under {sources}",
        hint="run generate_plant/build_creature first, or pass --out <file.blend>",
    )
