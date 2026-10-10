"""`render_growth_stages` -- growth/life stage comparison renders (stage 8).

Generates (or reuses) the per-stage source masters produced by
`generate_plant_variants` / `build_creature` and renders each stage at the same
fixed camera and lighting, so stage differences are attributable to
stage-dependent proportions and not to a moving camera.

Review-sheet assembly (a single montage image) is **not** performed here: it
needs ImageMagick/FFmpeg, which this machine may not have. Each stage PNG is
emitted individually and the manifest records the list; when no assembler is
available the command says so instead of pretending a sheet exists.
"""

from __future__ import annotations

import json
from pathlib import Path

from ..blender import pipeline as PL
from ..blender.params import render_settings, stage_names
from ..core.contract import Context, command
from ..core.errors import UsageError
from ..core.status import Status

PRESETS = ("neutral", "sun", "overcast", "wet")


@command("render_growth_stages",
         "Render a growth/life stage comparison sheet",
         stage="8 quality assurance")
def render_growth_stages(ctx: Context) -> Status:
    status = ctx.new_status("render_growth_stages")
    definition = ctx.load_definition(ctx.options.get("definition"))
    # A malformed definition is a usage error (exit 2), reported before the
    # external-tool check (exit 4); --dry-run never needs Blender at all.
    asset_id = ctx.options.get("asset_id") or definition.get("id") or "asset.unnamed"
    kind = str(definition.get("kind") or ("creature" if "body_plan" in definition else "plant"))

    requested = ctx.options.get("stage")
    stages = [requested] if requested else stage_names(definition)[:6]
    engine = str(ctx.options.get("engine") or definition.get("engine") or "EEVEE")
    settings = render_settings(definition, ctx, {"engine": engine})
    preset_name = str(definition.get("lighting_preset", "neutral"))
    preset_name = preset_name if preset_name in PRESETS else "neutral"
    angles = [int(a) for a in (definition.get("stage_angles") or [0, 90])][:4]

    preview = ctx.workspace.sub("preview", asset_id, "growth_stages")
    images_rel = str(preview.relative_to(ctx.workspace.root))
    result_rel = str((preview / "growth_stages.result.json").relative_to(ctx.workspace.root))
    outputs = PL.prepare_outputs(ctx, [images_rel, result_rel])
    outputs["images"] = outputs[images_rel]

    params = PL.base_params(ctx, command="render_growth_stages", asset_id=asset_id,
                            definition=definition, stage=requested,
                            variant=ctx.options.get("variant"), outputs=outputs,
                            engine=engine, resolution=settings["resolution"])
    params["presets"] = [preset_name]
    params["angles"] = angles
    params["elevation_deg"] = float(definition.get("turntable_elevation_deg", 12.0))
    params["samples"] = settings["samples"]
    params["ground"] = bool(definition.get("render_ground", True))
    params["render"] = settings

    if ctx.options.get("dry_run"):
        status.asset_id = asset_id
        status.metrics["plan"] = {"stages": stages, "kind": kind, "engine": engine,
                                  "presets": [preset_name], "angles": angles,
                                  "outputs": outputs}
        return status
    PL.require_blender(ctx)      # exit 4: no Blender means no run


    entries: list[dict] = []
    stage_rows: list[dict] = []
    for stage in stages:
        blend = _ensure_stage_master(ctx, definition, kind, asset_id, stage)
        run_outputs = dict(outputs)
        run_outputs["source_blend"] = str(blend)
        run_outputs["images"] = outputs[images_rel]
        stage_params = dict(params)
        stage_params["stage"] = stage
        stage_params["source_blend"] = str(blend)
        stage_params["seed"] = ctx.seed_for("render_growth_stages", asset_id, stage)
        stage_params["stem"] = f"{asset_id}_{stage}"
        stage_params["outputs"] = run_outputs

        _, result = PL.run_script(ctx, status, script="render", params=stage_params,
                                  log_name=f"render_growth_{asset_id}_{stage}")
        entries.extend(result.get("artifacts", []))
        stage_rows.append({
            "stage": stage,
            "source": str(blend),
            "renders": [a["path"] for a in result.get("artifacts", [])],
            "resolution": result.get("resolution"),
            "engine": result.get("engine"),
        })

    result_file = Path(outputs[result_rel])
    result_file.write_text(json.dumps({"asset_id": asset_id, "stages": stage_rows},
                                      indent=2, default=str), encoding="utf-8")
    entries.append({"path": str(result_file), "role": "preview", "kind": "result_json"})

    metrics = PL.record_metrics(status, result if stage_rows else {},
                                extra={"stages": stage_rows, "kind": kind,
                                       "presets": [preset_name], "angles": angles})
    status.metrics["renders"] = sum(len(r["renders"]) for r in stage_rows)
    paths = PL.add_artifacts(ctx, status, entries, tag="render_growth_stages")
    status.warn("per-stage PNGs only: montage assembly needs ImageMagick/FFmpeg "
                "(absent here), so no single sheet image is claimed")

    PL.write_manifest(
        ctx, status, asset_id=f"{asset_id}.growth", kind="scene",
        command="render_growth_stages", artifacts=paths, metrics=metrics,
        result=stage_rows[-1] if stage_rows else {}, parent_asset_id=asset_id,
        notes=(f"growth stages {stages} rendered at identical fixed camera angles "
               f"{angles} and preset '{preset_name}' so proportion differences are "
               "attributable to stage parameters"),
    )
    status.asset_id = f"{asset_id}.growth"
    return status


def _ensure_stage_master(ctx: Context, definition: dict, kind: str, asset_id: str,
                         stage: str) -> Path:
    """Return a .blend for this stage, generating it when it does not exist yet.

    Order matters: a stage master is *never* silently replaced by the adult
    master, because that would render every stage with identical proportions
    and make the comparison sheet meaningless. The unsuffixed master is only a
    last resort, and the caller is told through the returned path.
    """
    sources = ctx.workspace.sub("sources", asset_id)
    direct = sources / f"{asset_id}.{stage}.blend"
    if direct.is_file():
        return direct
    if kind == "creature":
        from .build_creature import build_creature

        build_creature(_child_ctx(ctx, stage))
    else:
        from .generate_plant import generate_plant

        generate_plant(_child_ctx(ctx, stage))
    if direct.is_file():
        return direct
    plain = sources / f"{asset_id}.blend"
    if plain.is_file():
        ctx.status.warn(
            f"no per-stage master for '{stage}'; fell back to the unsuffixed master "
            f"{plain.name}, so this stage's render may not show stage proportions")
        return plain
    raise UsageError(
        f"could not produce a source master for stage '{stage}' under {sources}",
        hint="run generate_plant/build_creature for this stage first",
    )


def _child_ctx(ctx: Context, stage: str) -> Context:
    from ..core.contract import Context as Ctx

    options = dict(ctx.options)
    options["stage"] = stage
    return Ctx(workspace=ctx.workspace, seed=ctx.seed, options=options)
