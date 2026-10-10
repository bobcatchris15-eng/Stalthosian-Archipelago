"""`benchmark_asset` -- canonical pipeline stage 8 (quality assurance).

Measures the runtime asset and writes `var/reports/benchmark/<asset_id>.json`.

**Measure, never infer.** Everything counted here is decoded from the shipped
files or read from the filesystem. Three things cannot be measured by a toolkit
that ships no renderer, and they are reported as not measured with a reason:

* frame time (`frame_ms_p50/p95/p99`, `fps`),
* driver-side VRAM residency (a buffer-view lower bound is reported instead),
* streaming wall-clock cost.

The budget every number is measured against is stated in the report: **GTX 1080
8 GB, 1920x1080, 30 FPS floor = 33.33 ms for the whole scene**.

Exit codes: 0 measured (with honest unmeasured gaps), 2 nothing to measure,
3 the asset could not be read, 7 timeout.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from .. import __version__
from ..core import Context, Status, command
from ..core.errors import EXIT_OK, UsageError, ValidationFailed
from ..optimize.glb import GlbError
from ..optimize.pipeline_state import (
    GEOMETRY_SUFFIXES,
    artifacts,
    asset_id_from,
    load_manifest,
    metrics_map,
    resolve,
    resolve_manifest_path,
    update_manifest,
)
from ..benchmark.measure import (
    BenchmarkReport,
    Measurement,
    compare_before_after,
    frame_time_measurement,
    measure_glb,
    streaming_cost,
    vram_estimate,
)


@command("benchmark_asset", "Measure geometry, texture, memory and frame-time cost",
         stage="8 quality assurance")
def benchmark_asset(ctx: Context) -> Status:
    status = ctx.new_status("benchmark_asset")
    if ctx.options.get("definition"):
        ctx.load_definition(ctx.options.get("definition"))

    manifest_path = resolve_manifest_path(ctx)
    manifest = load_manifest(manifest_path)
    asset_id = asset_id_from(ctx, manifest)
    status.asset_id = asset_id
    root = ctx.workspace.root

    entries = [a for a in artifacts(manifest)
               if str(a.get("path", "")).lower().endswith(GEOMETRY_SUFFIXES)]
    if not entries:
        raise UsageError(
            f"manifest {manifest_path.name} declares no glTF/GLB artifact to measure",
            hint="run generate_lods (stage 6) first")

    report = BenchmarkReport(asset_id=asset_id)
    per_lod_resident: dict[str, int] = {}
    per_lod_bytes: dict[str, int] = {}

    for entry in entries:
        path = resolve(str(entry.get("path", "")), root)
        try:
            measurement = measure_glb(path)
        except GlbError as exc:
            raise ValidationFailed(f"cannot measure {path}: {exc.message}") from exc
        name = _level_name(path)
        report.lods.append(measurement)
        per_lod_bytes[name] = measurement["file_bytes"]
        per_lod_resident[name] = measurement["resident_bytes"]
        for item in measurement["measurements"]:
            report.add_measurement(Measurement(item["name"], item["value"], item["unit"],
                                               bool(item["measured"]), item["method"]))

    texture_artifact_bytes = 0
    for entry in artifacts(manifest):
        if str(entry.get("role", "")) == "texture":
            path = resolve(str(entry.get("path", "")), root)
            if path.exists():
                texture_artifact_bytes += path.stat().st_size
    texture_bytes = texture_artifact_bytes

    # before / after against the source asset, when one is still recorded
    source_entry = next((a for a in artifacts(manifest)
                         if str(a.get("role", "")) == "source"
                         and str(a.get("path", "")).lower().endswith(GEOMETRY_SUFFIXES)), None)
    if source_entry is not None:
        source_path = resolve(str(source_entry.get("path", "")), root)
        if source_path.exists():
            before = measure_glb(source_path)
            after = next((m for m in report.lods if Path(m["path"]).name == "lod0.glb"), None)
            if after is not None:
                for measurement in compare_before_after(before, after):
                    report.add_measurement(measurement)
                report.warnings.append(
                    "before/after compares the source asset with LOD0; silhouette drift is NOT "
                    "measured here (it needs a rendered comparison)")

    report.frame_time = frame_time_measurement(timeout=ctx.options.get("timeout"))
    report.memory = {
        "vram_lower_bound": vram_estimate(per_lod_resident, texture_bytes),
        "streaming": streaming_cost(per_lod_bytes),
        "per_lod_resident_bytes": per_lod_resident,
        "texture_bytes": texture_bytes,
    }
    if report.frame_time.get("measured") is False:
        report.warnings.append(report.frame_time.get("reason", "frame time not measured"))

    recorded = metrics_map(manifest)
    for name in ("texture_bytes_before", "texture_bytes_after"):
        if name in recorded:
            report.add_measurement(Measurement(f"source.{name}", float(recorded[name]["value"]),
                                               recorded[name].get("unit", "bytes"),
                                               bool(recorded[name].get("measured", True)),
                                               "recorded by generate_lods"))

    payload = report.to_dict()
    payload["tool"] = {"name": "stalthos benchmark_asset", "version": __version__,
                       "blender_required": False, "seed": ctx.seed}
    payload["manifest_path"] = str(manifest_path)

    status.metrics = {
        "asset_id": asset_id,
        "lod_count": len(report.lods),
        "lods": [{"name": _level_name(Path(m["path"])), "triangles": m["summary"]["triangles"],
                  "vertices": m["summary"]["vertices"], "materials": m["summary"]["materials"],
                  "textures": m["summary"]["textures"], "file_bytes": m["file_bytes"],
                  "resident_bytes": m["resident_bytes"]} for m in report.lods],
        "frame_time_measured": bool(report.frame_time.get("measured")),
        "frame_budget_ms": 33.33,
        "reference_gpu": "NVIDIA GTX 1080 8 GB",
        "texture_bytes": texture_bytes,
        "vram_lower_bound_bytes": report.memory["vram_lower_bound"]["value"],
        "vram_is_estimate": True,
        "report_path": str(ctx.workspace.reports / "benchmark" / f"{asset_id}.json"),
    }
    for message in report.warnings:
        status.warn(message)

    status.provenance = {
        "generator": "stalthos-toolkit",
        "toolkit_version": __version__,
        "tool_versions": {"python": ".".join(str(p) for p in __import__("sys").version_info[:3]),
                          "external": "none-required"},
        "command": "benchmark_asset",
        "seed": ctx.seed,
        "input_hashes": {str(manifest_path): ctx.artifact(manifest_path).sha256 or ""},
    }

    if ctx.options.get("dry_run"):
        status.log("dry-run: no report written, manifest not updated")
        return status

    report_path = ctx.workspace.reports / "benchmark" / f"{asset_id}.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(payload, indent=2, default=str) + "\n", encoding="utf-8")
    status.add_artifact(ctx.artifact(report_path, role="report"))

    metrics = [
        {"name": "frame_ms_p99", "value": 0.0, "unit": "ms", "measured": False},
        {"name": "vram_lower_bound_bytes", "value": report.memory["vram_lower_bound"]["value"],
         "unit": "bytes", "measured": False},
        {"name": "streaming_wall_clock_ms", "value": 0.0, "unit": "ms", "measured": False},
        {"name": "draw_calls", "value": float(sum(m["summary"]["draw_calls"] for m in report.lods)),
         "unit": "calls", "measured": True},
    ]
    updated = update_manifest(manifest_path, manifest, metrics=metrics)
    status.add_artifact(ctx.artifact(updated, role="report"))

    status.exit_code = EXIT_OK
    status.ok = True
    return status


def _level_name(path: Path) -> str:
    stem = path.stem.lower()
    return stem if stem else path.name


__all__ = ["benchmark_asset"]