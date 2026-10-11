"""`build_creature` -- anatomical proxy geometry (pipeline stage 3).

Explicitly a **pipeline-verification proxy**: a rigged-able quadruped body plan
(segmented spine, neck, skull, tail, four limbs) driven by the definition's
proportion parameters. It is not a skeletal reconstruction and carries no
paleontological claim; the disclaimer travels in the result JSON, the status
envelope and the manifest notes.
"""

from __future__ import annotations

import json
from pathlib import Path

from ..blender import pipeline as PL
from ..blender.params import creature_shape_params, creature_surface_params
from ..core.contract import Context, command
from ..core.errors import NotImplementedStage
from ..core.status import Status

PROXY_NOTE = ("anatomical proxy for pipeline verification; proportions are definition "
              "parameters, not fossil measurements")

#: Only a quadrupedal body plan is implemented. The bpy script builds a
#: segmented-spine quadruped; emitting one for a biped or a serpentine plan would
#: be a confidently wrong asset, so those exit 5 instead (AGENTS.md rule 1).
SUPPORTED_POSTURES = {"quadrupedal", "cursorial_quadruped"}


@command("build_creature",
         "Build creature anatomical proxy geometry from a creature definition",
         stage="3 high-resolution generation")
def build_creature(ctx: Context) -> Status:
    status = ctx.new_status("build_creature")
    definition = ctx.load_definition(ctx.options.get("definition"))
    PL.require_kind(definition, "creature", "build_creature")
    # A malformed definition is a usage error (exit 2), reported before the
    # external-tool check (exit 4); --dry-run never needs Blender at all.
    asset_id = ctx.options.get("asset_id") or definition.get("id") or "creature.unnamed"
    stage = ctx.options.get("stage")

    shape = creature_shape_params(definition, stage=stage)
    surface = creature_surface_params(definition)
    posture = str(shape.get("posture") or "quadrupedal")
    if posture not in SUPPORTED_POSTURES:
        raise NotImplementedStage(
            f"build_creature implements a quadrupedal proxy only; "
            f"this definition declares posture={posture!r}",
            detail={"generator": definition.get("generator"), "posture": posture,
                    "implemented_postures": sorted(SUPPORTED_POSTURES)},
            hint="use a quadrupedal definition, or have the quadruped proxy extended "
                 "to this body plan before running the command",
        )
    if ctx.options.get("dry_run"):
        status.asset_id = asset_id
        status.metrics.update({"plan": shape, "surface": surface, "note": PROXY_NOTE})
        return status
    PL.require_blender(ctx)      # exit 4: no Blender means no run


    suffix = f".{stage}" if stage else ""
    sources = ctx.workspace.sub("sources", asset_id)
    blend_rel = str((sources / f"{asset_id}{suffix}.blend").relative_to(ctx.workspace.root))
    result_rel = str((sources / f"{asset_id}{suffix}.result.json").relative_to(ctx.workspace.root))
    outputs = PL.prepare_outputs(ctx, [blend_rel, result_rel])
    outputs["blend"] = outputs[blend_rel]
    outputs["result_json"] = outputs[result_rel]

    params = PL.base_params(ctx, command="build_creature", asset_id=asset_id,
                            definition=definition, stage=stage,
                            variant=ctx.options.get("variant"), outputs=outputs)
    params["creature"] = shape
    params["surface"] = surface
    params["seed"] = ctx.seed_for("build_creature", asset_id, stage or "adult")

    _, result = PL.run_script(ctx, status, script="creature", params=params,
                              log_name=f"build_creature_{asset_id}{suffix}")

    result_file = Path(outputs[result_rel])
    result_file.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")

    written = list(result.get("artifacts", [])) + [
        {"path": str(result_file), "role": "source", "kind": "result_json"}
    ]
    metrics = PL.record_metrics(status, result, extra={"proxy_note": PROXY_NOTE,
                                                      "stage": stage or "adult",
                                                      "posture": posture,
                                                      "proxy_approximation": True})
    paths = PL.add_artifacts(ctx, status, written, tag="build_creature")
    status.metrics["source_blend"] = paths[0] if paths else ""
    status.warn(PROXY_NOTE)

    PL.write_manifest(
        ctx, status, asset_id=asset_id, kind="creature", command="build_creature",
        artifacts=paths, metrics=metrics, result=result,
        notes=PROXY_NOTE,
    )
    status.asset_id = asset_id
    return status
