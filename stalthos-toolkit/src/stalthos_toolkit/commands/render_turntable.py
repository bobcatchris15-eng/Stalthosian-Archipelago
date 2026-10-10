"""`render_turntable` -- deterministic review renders (pipeline stage 8).

Fixed azimuths (0/90/180/270 by default), fixed elevation, bounding-box framing
with a constant fit formula, and fixed light presets (neutral plus cheap
sun/overcast/wet variants). EEVEE is the default engine because it is fast
enough to actually run unattended; ``--engine CYCLES`` is a one-option switch.

Renders land in ``var/preview/<asset_id>/`` as hashed artifacts.
"""

from __future__ import annotations

import json
from pathlib import Path

from ..blender import pipeline as PL
from ..blender.params import render_settings
from ..core.contract import Context, command
from ..core.errors import UsageError
from ..core.status import Status

PRESETS = ("neutral", "sun", "overcast", "wet")


@command("render_turntable",
         "Render a deterministic turntable review sheet",
         stage="8 quality assurance")
def render_turntable(ctx: Context) -> Status:
    status = ctx.new_status("render_turntable")
    definition = ctx.load_definition(ctx.options.get("definition"))
    # A malformed definition is a usage error (exit 2), reported before the
    # external-tool check (exit 4); --dry-run never needs Blender at all.
    asset_id = ctx.options.get("asset_id") or definition.get("id") or "asset.unnamed"
    stage = ctx.options.get("stage")
    suffix = f".{stage}" if stage else ""

    PL.require_blender(ctx)      # exit 4: no Blender means no run
    source = _resolve_source(ctx, asset_id, suffix)
    engine = str(ctx.options.get("engine") or definition.get("engine") or "EEVEE")
    settings = render_settings(definition, ctx, {"engine": engine,
                                                 "preset": ctx.options.get("variant") or "neutral"})
    presets = _requested_presets(ctx, definition)
    angles = _requested_angles(ctx, definition)

    preview = ctx.workspace.sub("preview", asset_id, "turntable")
    images_rel = str(preview.relative_to(ctx.workspace.root))
    result_rel = str((preview / f"{asset_id}{suffix}.turntable.result.json")
                     .relative_to(ctx.workspace.root))
    outputs = PL.prepare_outputs(ctx, [images_rel, result_rel])
    outputs["images"] = outputs[images_rel]

    params = PL.base_params(ctx, command="render_turntable", asset_id=asset_id,
                            definition=definition, stage=stage,
                            variant=ctx.options.get("variant"), outputs=outputs,
                            engine=engine, resolution=settings["resolution"])
    params["source_blend"] = str(source)
    params["seed"] = settings["seed"]
    params["presets"] = presets
    params["angles"] = angles
    params["elevation_deg"] = float(definition.get("turntable_elevation_deg", 12.0))
    params["samples"] = settings["samples"]
    params["ground"] = bool(definition.get("render_ground", True))
    params["render"] = settings
    params["stem"] = f"{asset_id}{suffix}"

    if ctx.options.get("dry_run"):
        status.asset_id = asset_id
        status.metrics["plan"] = {"source": str(source), "presets": presets,
                                  "angles": angles, "engine": engine,
                                  "resolution": settings["resolution"], "outputs": outputs}
        return status


    _, result = PL.run_script(ctx, status, script="render", params=params,
                              log_name=f"render_turntable_{asset_id}{suffix}")

    result_file = Path(outputs[result_rel])
    result_file.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
    written = list(result.get("artifacts", [])) + [
        {"path": str(result_file), "role": "preview", "kind": "result_json"}
    ]
    metrics = PL.record_metrics(status, result, extra={"source_blend": str(source),
                                                      "presets": presets, "angles": angles})
    paths = PL.add_artifacts(ctx, status, written, tag="render_turntable")

    PL.write_manifest(
        ctx, status, asset_id=f"{asset_id}.turntable", kind="scene",
        command="render_turntable", artifacts=paths, metrics=metrics, result=result,
        parent_asset_id=asset_id,
        notes=(f"deterministic turntable: angles={angles}, presets={presets}, "
               f"engine={result.get('engine')}, fixed sun/world, no animated seed"),
    )
    status.asset_id = f"{asset_id}.turntable"
    return status


def _requested_presets(ctx: Context, definition: dict) -> list[str]:
    variant = ctx.options.get("variant")
    if variant:
        wanted = [p.strip() for p in str(variant).replace("+", ",").split(",") if p.strip()]
        return [p for p in wanted if p in PRESETS] or ["neutral"]
    presets = definition.get("lighting_presets")
    if isinstance(presets, list) and presets and all(str(p) in PRESETS for p in presets):
        return [str(p) for p in presets]
    single = str(definition.get("lighting_preset", "neutral"))
    return [single if single in PRESETS else "neutral"]


def _requested_angles(ctx: Context, definition: dict) -> list[int]:
    angles = definition.get("turntable_angles")
    if isinstance(angles, list) and angles:
        return [int(a) for a in angles][:16]
    return [0, 90, 180, 270]


def _resolve_source(ctx: Context, asset_id: str, suffix: str) -> Path:
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
    candidates = [c for c in candidates if ".baked." not in c.name]
    if candidates:
        return candidates[0]
    raise UsageError(
        f"no source .blend found under {sources}",
        hint="run generate_plant/build_creature first, or pass --out <file.blend>",
    )
