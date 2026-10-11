"""`rig_creature` -- armature, automatic weights, neutral pose export (stage 3).

Consumes the .blend master produced by `build_creature`, rebuilds the same
proportion-derived skeleton (so bones land exactly on the proxy geometry), applies
automatic weights, validates the neutral pose and exports a GLB neutral pose.
Bone counts and any skinning warnings are recorded in the result JSON, the
status envelope and the manifest.
"""

from __future__ import annotations

import json
from pathlib import Path

from ..blender import pipeline as PL
from ..blender.params import creature_shape_params, creature_surface_params
from ..core.contract import Context, command
from ..core.errors import UsageError
from ..core.status import Status

PROXY_NOTE = ("anatomical proxy rig for pipeline verification; proportions are definition "
              "parameters, not fossil measurements")


@command("rig_creature",
         "Create/refresh the authored rig and export neutral poses",
         stage="3 high-resolution generation")
def rig_creature(ctx: Context) -> Status:
    status = ctx.new_status("rig_creature")
    definition = ctx.load_definition(ctx.options.get("definition"))
    PL.require_kind(definition, "creature", "rig_creature")
    # A malformed definition is a usage error (exit 2), reported before the
    # external-tool check (exit 4); --dry-run never needs Blender at all.
    asset_id = ctx.options.get("asset_id") or definition.get("id") or "creature.unnamed"
    stage = ctx.options.get("stage")
    shape = creature_shape_params(definition, stage=stage)

    suffix = f".{stage}" if stage else ""
    sources = ctx.workspace.sub("sources", asset_id)
    source = sources / f"{asset_id}{suffix}.blend"

    if ctx.options.get("dry_run"):
        status.asset_id = asset_id
        status.metrics["plan"] = {"source": str(source), "creature": shape,
                                  "source_exists": source.is_file()}
        return status

    PL.require_blender(ctx)      # exit 4: no Blender means no run
    if not source.is_file():
        raise UsageError(
            f"creature source master not found: {source}",
            hint="run `stalthos build_creature` first, or pass --asset-id/--stage to match it",
        )

    rig_rel = str((sources / f"{asset_id}{suffix}.rig.blend").relative_to(ctx.workspace.root))
    result_rel = str((sources / f"{asset_id}{suffix}.rig.result.json").relative_to(ctx.workspace.root))
    neutral_rel = str((ctx.workspace.runtime / "creatures" / asset_id /
                       f"{asset_id}{suffix}.neutral.glb").relative_to(ctx.workspace.root))
    outputs = PL.prepare_outputs(ctx, [rig_rel, result_rel, neutral_rel])
    outputs["neutral_pose"] = outputs[neutral_rel]
    outputs["blend"] = outputs[rig_rel]
    outputs["source"] = str(source)

    params = PL.base_params(ctx, command="rig_creature", asset_id=asset_id,
                            definition=definition, stage=stage,
                            variant=ctx.options.get("variant"), outputs=outputs)
    params["creature"] = shape
    params["surface"] = creature_surface_params(definition)
    params["seed"] = ctx.seed_for("rig_creature", asset_id, stage or "adult")
    params["source_blend"] = str(source)

    _, result = PL.run_script(ctx, status, script="rig", params=params,
                              log_name=f"rig_creature_{asset_id}{suffix}")

    result_file = Path(outputs[result_rel])
    result_file.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")

    written = list(result.get("artifacts", [])) + [
        {"path": str(result_file), "role": "source", "kind": "result_json"}
    ]
    metrics = PL.record_metrics(status, result, extra={"proxy_note": PROXY_NOTE,
                                                      "source_blend": str(source)})
    paths = PL.add_artifacts(ctx, status, written, tag="rig_creature")

    pose = (result.get("metrics", {}) or {}).get("pose") or {}
    if not pose.get("neutral", True):
        status.warn("neutral pose validation reported offset bones")

    PL.write_manifest(
        ctx, status, asset_id=f"{asset_id}.rig", kind="creature", command="rig_creature",
        artifacts=paths, metrics=metrics, result=result, parent_asset_id=asset_id,
        notes=(f"{PROXY_NOTE}; weighting={metrics.get('weighting')}; "
               f"bones={metrics.get('bone_count')}"),
    )
    status.asset_id = f"{asset_id}.rig"
    return status
