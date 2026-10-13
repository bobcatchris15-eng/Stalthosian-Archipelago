"""`generate_plant` -- canonical plant source geometry (pipeline stage 3).

Drives the definition's morphology family through one of five distinct
generators (branch-aware tree, true rachis/pinna fern, segmented horsetail,
rosette, broadleaf shrub). Output is an **editable** .blend master under
``var/sources/<asset_id>/`` plus a metrics-rich result JSON; no destructive-only
output, no file outside the workspace.
"""

from __future__ import annotations

import json
from pathlib import Path

from ..blender import pipeline as PL
from ..blender.params import apply_scales, growth_stage_list, growth_stage_params, normalize_plant_params
from ..core.contract import Context, command
from ..core.errors import UsageError
from ..core.status import Status


@command("generate_plant",
         "Generate canonical plant source geometry from a species definition",
         stage="3 high-resolution generation")
def generate_plant(ctx: Context) -> Status:
    status = ctx.new_status("generate_plant")
    definition = ctx.load_definition(ctx.options.get("definition"))
    PL.require_kind(definition, "plant", "generate_plant")
    # A malformed definition is a usage error (exit 2), reported before the
    # external-tool check (exit 4); --dry-run never needs Blender at all.
    asset_id = ctx.options.get("asset_id") or definition.get("id") or "plant.unnamed"
    stage = ctx.options.get("stage")
    variant = ctx.options.get("variant")

    family = PL.P.resolve_family(definition)
    payload = normalize_plant_params(definition, ctx, family=family, stage=stage,
                                     asset_id=asset_id)
    stage_block = growth_stage_params(definition, stage=stage)
    if stage:
        if not stage_block.get("available"):
            raise UsageError(f"invalid plant growth stage {stage!r}")
        # Apply existing growth scales once. Preserve canonical variant and
        # no-stage behavior; variant generation owns its own multipliers.
        original_shape = payload["plant"]
        scaled_shape = apply_scales(original_shape, {}, stage_block)
        declared = next(row for row in growth_stage_list(definition) if row["id"] == stage)
        if declared.get("height_m") is not None:
            scaled_shape["height_m"] = declared["height_m"]
        # Legacy mappings can already supply absolute per-stage shape values.
        # Keep those parsed values instead of scaling their dimensions twice.
        raw_stages = definition.get("growth_stages")
        if isinstance(raw_stages, dict):
            for key in raw_stages.get(stage, {}):
                if key in original_shape and key != "height_m":
                    scaled_shape[key] = original_shape[key]
        payload["plant"] = scaled_shape
    if ctx.options.get("dry_run"):
        status.asset_id = asset_id
        status.metrics["plan"] = payload
        return status

    PL.require_blender(ctx)      # exit 4: no Blender means no run

    suffix = f".{stage}" if stage else ""
    sources = ctx.workspace.sub("sources", asset_id)
    blend_rel = str((sources / f"{asset_id}{suffix}.blend").relative_to(ctx.workspace.root))
    result_rel = str((sources / f"{asset_id}{suffix}.result.json").relative_to(ctx.workspace.root))
    outputs = PL.prepare_outputs(ctx, [blend_rel, result_rel])
    outputs["blend"] = outputs[blend_rel]
    outputs["result_json"] = outputs[result_rel]

    params = PL.base_params(ctx, command="generate_plant", asset_id=asset_id,
                            definition=definition, stage=stage, variant=variant,
                            outputs=outputs)
    params.update(payload)
    params["growth_stage"] = stage_block

    _, result = PL.run_script(ctx, status, script="plants", params=params,
                              log_name=f"generate_plant_{asset_id}{suffix}")

    result_file = Path(outputs[result_rel])
    result_file.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")

    written = list(result.get("artifacts", [])) + [
        {"path": str(result_file), "role": "source", "kind": "result_json"}
    ]
    metrics = PL.record_metrics(status, result,
                                extra={"family": family, "stage": stage or "adult"})
    paths = PL.add_artifacts(ctx, status, written, tag="generate_plant")
    status.metrics["source_blend"] = paths[0] if paths else ""

    PL.write_manifest(
        ctx, status, asset_id=asset_id, kind="plant", command="generate_plant",
        artifacts=paths, metrics=metrics, result=result,
        notes=(f"morphology family '{family}' generated as an editable .blend master "
               "(unapplied modifiers, named objects, per-organ collections)"),
    )

    status.asset_id = asset_id
    return status
