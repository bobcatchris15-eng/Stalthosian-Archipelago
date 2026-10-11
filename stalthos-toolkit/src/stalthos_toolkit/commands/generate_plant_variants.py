"""`generate_plant_variants` -- seeded variants and growth stages (stage 5).

Each variant index gets its own derived seed (``ctx.seed_for``); each growth
stage gets stage-dependent proportions from the definition. Variation ranges are
bounded by the definition's own ``variation`` block (default +-12%) so a family
still reads as one lineage instead of collapsing into unrelated silhouettes.
"""

from __future__ import annotations

import json
from pathlib import Path

from ..blender import pipeline as PL
from ..blender.params import (
    apply_scales,
    growth_stage_params,
    normalize_plant_params,
    plant_variation_params,
    plant_shape_params,
    stage_names,
    variant_multipliers,
    variant_names,
)
from ..core.contract import Context, command
from ..core.status import Status


@command("generate_plant_variants",
         "Generate seeded morphological variants and growth stages",
         stage="5 variants/stages")
def generate_plant_variants(ctx: Context) -> Status:
    status = ctx.new_status("generate_plant_variants")
    definition = ctx.load_definition(ctx.options.get("definition"))
    PL.require_kind(definition, "plant", "generate_plant_variants")
    # A malformed definition is a usage error (exit 2), reported before the
    # external-tool check (exit 4); --dry-run never needs Blender at all.
    base_id = ctx.options.get("asset_id") or definition.get("id") or "plant.unnamed"
    family = PL.P.resolve_family(definition)

    requested_stage = ctx.options.get("stage")
    stages = [requested_stage] if requested_stage else stage_names(definition)[:4]
    count = int(ctx.options.get("count") or 2)
    count = max(1, min(count, 16))
    variants = variant_names(definition, count)[:count]

    plan = {
        "family": family,
        "stages": stages,
        "variants": variants,
        "count": count,
        "variation_bounds": {
            k: v for k, v in plant_variation_params(definition, family, variant_index=0).items()
            if isinstance(v, tuple)
        },
    }
    if ctx.options.get("dry_run"):
        status.asset_id = base_id
        status.metrics["plan"] = plan
        return plan and status
    PL.require_blender(ctx)      # exit 4: no Blender means no run


    sources = ctx.workspace.sub("sources", base_id, "variants")
    outputs = {"blend": "", "result": ""}
    entries: list[dict] = []
    results: list[dict] = []
    totals = {"triangles": 0, "vertices": 0}

    for v_index, variant in enumerate(variants):
        for stage in stages:
            name = f"{base_id}.{variant}.{stage}"
            blend_rel = str((sources / f"{name}.blend").relative_to(ctx.workspace.root))
            result_rel = str((sources / f"{name}.result.json").relative_to(ctx.workspace.root))
            run_outputs = PL.prepare_outputs(ctx, [blend_rel, result_rel])
            run_outputs["blend"] = run_outputs[blend_rel]
            run_outputs["result_json"] = run_outputs[result_rel]

            seed = ctx.seed_for("generate_plant_variants", base_id, variant, stage)
            shape = plant_shape_params(definition, family, stage=stage)
            variation = plant_variation_params(definition, family, variant_index=v_index)
            multipliers = variant_multipliers(ctx, name, variation, v_index)
            stage_block = growth_stage_params(definition, stage=stage)
            shape = apply_scales(shape, multipliers, stage_block)

            payload = normalize_plant_params(definition, ctx, family=family,
                                             variant_index=v_index, stage=stage,
                                             asset_id=name)
            payload["plant"] = shape
            payload["seed"] = seed
            payload["variant"] = variant
            payload["outputs"] = run_outputs

            params = PL.base_params(ctx, command="generate_plant_variants", asset_id=name,
                                    definition=definition, stage=stage, variant=variant,
                                    outputs=run_outputs)
            params.update(payload)

            _, result = PL.run_script(ctx, status, script="plants", params=params,
                                      log_name=f"variant_{name}")
            result_file = Path(run_outputs[result_rel])
            result_file.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")

            entries.extend(result.get("artifacts", []))
            entries.append({"path": str(result_file), "role": "source", "kind": "result_json"})
            totals["triangles"] += int(result.get("metrics", {}).get("triangles", 0) or 0)
            totals["vertices"] += int(result.get("metrics", {}).get("vertices", 0) or 0)
            results.append({
                "variant": variant, "stage": stage, "seed": seed,
                "asset_id": name,
                "multipliers": {k: round(v, 6) for k, v in multipliers.items()},
                "triangles": result.get("metrics", {}).get("triangles"),
                "height_m": result.get("metrics", {}).get("height_m"),
            })

    paths = PL.add_artifacts(ctx, status, entries, tag="generate_plant_variants")
    status.metrics.update({
        "family": family,
        "variants": variants,
        "stages": stages,
        "results": results,
        "triangles": totals["triangles"],
        "vertices": totals["vertices"],
        "lineage_note": ("variation is bounded by the definition's variation ranges "
                         "(default +-12%) so variants stay one lineage"),
    })

    PL.write_manifest(
        ctx, status, asset_id=f"{base_id}.variants", kind="plant",
        command="generate_plant_variants", artifacts=paths,
        metrics=status.metrics, result=results[-1] if results else {},
        notes=f"{len(variants)} variants x {len(stages)} growth stages, each on its own derived seed",
    )
    status.asset_id = f"{base_id}.variants"
    return status
