"""Measurement harness for stage 8 (benchmark_asset).

Rule: **measure, never infer**. Every number in the report carries a `measured`
flag, and anything this harness cannot actually observe on this machine is
reported as not measured with a reason. A missing GPU profiler is not a reason
to invent a frame time.

What IS measured here (offline, from the files on disk):

* per-LOD triangle / vertex / material / texture / image / draw-call counts
  (decoded from the GLB),
* per-LOD file bytes and the resident size of every live buffer view,
* total chain bytes, collision proxy cost, texture bytes before/after.

What is NOT measured without a runtime (and is therefore flagged):

* GPU frame time (needs PresentMon/RenderDoc **and** a running scene),
* VRAM residency as the driver allocates it (the buffer-view sum is a lower
  bound on what the driver will hold, not the driver's own accounting),
* streaming wall-clock cost (needs a measured disk/IO path).

Budget the report is measured against
--------------------------------------
``GTX_1080_BUDGET``: NVIDIA GTX 1080 8 GB, 1920x1080 native, **30 FPS floor
= 33.33 ms/frame** for the entire scene (see
``Rauken-Archipelago/docs/rendering-experiments.md``).
"""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

from ..optimize import tools as toolbox
from ..optimize.glb import GlbDocument, GlbError, rig_summary


@dataclass(frozen=True)
class ReferenceBudget:
    """The machine and frame-time target this project is measured against."""

    name: str = "NVIDIA GTX 1080 8 GB"
    cpu: str = "first-generation Ryzen"
    resolution: tuple[int, int] = (1920, 1080)
    fps_floor: int = 30
    vram_gb: int = 8
    source: str = "Rauken-Archipelago/docs/rendering-experiments.md"

    @property
    def frame_ms_budget(self) -> float:
        return round(1000.0 / self.fps_floor, 2)

    @property
    def frame_ms_target_60(self) -> float:
        return round(1000.0 / 60.0, 2)

    def to_dict(self) -> dict:
        data = asdict(self)
        data["resolution"] = list(self.resolution)
        data["frame_ms_budget"] = self.frame_ms_budget
        data["frame_ms_60fps"] = self.frame_ms_target_60
        data["frame_budget_note"] = (
            f"{self.fps_floor} FPS = {self.frame_ms_budget} ms for the WHOLE scene, not per asset")
        return data


GTX_1080_BUDGET = ReferenceBudget()


@dataclass
class Measurement:
    """One number, with the honesty flag that says how it was obtained."""

    name: str
    value: float
    unit: str = ""
    measured: bool = True
    method: str = ""

    def to_dict(self) -> dict:
        return {"name": self.name, "value": self.value, "unit": self.unit,
                "measured": self.measured, "method": self.method}


def _live_buffer_bytes(doc: GlbDocument, accessor_indices: set[int]) -> int:
    views = doc.gltf.get("bufferViews") or []
    accessors = doc.gltf.get("accessors") or []
    view_indices = {int(accessors[i]["bufferView"]) for i in accessor_indices
                    if i < len(accessors) and "bufferView" in accessors[i]}
    total = 0
    for view_index in view_indices:
        if view_index < len(views):
            total += int(views[view_index].get("byteLength", 0))
    return total


def _primitive_accessor_indices(doc: GlbDocument) -> set[int]:
    used: set[int] = set()
    for _, _, primitive in doc.iter_primitives():
        used.update(int(v) for v in (primitive.get("attributes") or {}).values())
        if "indices" in primitive:
            used.add(int(primitive["indices"]))
    return used


def resident_bytes(path: str | Path) -> int:
    """Bytes of buffer views that actually feed this file's primitives.

    This is the geometry floor of what a runtime must hold resident. Driver-side
    allocation is larger; report this number as `measured=False` for VRAM.
    """
    doc = GlbDocument.read(path)
    return _live_buffer_bytes(doc, _primitive_accessor_indices(doc))


def measure_glb(path: str | Path) -> dict[str, Any]:
    """Counts and byte sizes read out of one GLB. All measured=True."""
    path = Path(path)
    if not path.exists():
        raise GlbError(f"asset not found: {path}")
    doc = GlbDocument.read(path)
    summary = doc.summary()
    resident = _live_buffer_bytes(doc, _primitive_accessor_indices(doc))
    file_bytes = path.stat().st_size

    lod_index = _lod_index(path.name)
    prefix = f"lod{lod_index}." if lod_index is not None else ""
    measurements = [
        Measurement(f"{prefix}triangles", summary.triangles, "triangles", True, "decoded from GLB accessors"),
        Measurement(f"{prefix}vertices", summary.vertices, "vertices", True, "decoded from GLB accessors"),
        Measurement(f"{prefix}materials", doc.unique_material_count(), "materials", True, "distinct material indices"),
        Measurement(f"{prefix}textures", summary.textures, "textures", True, "GLB texture count"),
        Measurement(f"{prefix}images", summary.images, "images", True, "GLB image count"),
        Measurement(f"{prefix}draw_calls", summary.draw_calls, "calls", True,
                    "one per triangle primitive; measured as geometry, NOT as an engine draw count"),
        Measurement(f"{prefix}mesh_bytes", float(resident), "bytes", True,
                    "sum of live buffer views feeding primitives"),
        Measurement(f"{prefix}file_bytes", float(file_bytes), "bytes", True, "file size on disk"),
    ]
    return {
        "path": str(path),
        "name": path.stem,
        "file_bytes": file_bytes,
        "resident_bytes": resident,
        "summary": summary.to_dict(),
        "rig": rig_summary(doc),
        "measurements": [m.to_dict() for m in measurements],
    }


def _lod_index(name: str) -> int | None:
    stem = name.lower()
    if stem.startswith("lod"):
        tail = stem[3:].split(".")[0]
        if tail.isdigit():
            return int(tail)
    return None


def frame_time_measurement(*, timeout: float | None = None) -> dict[str, Any]:
    """Try to measure frame time; report honestly when it cannot be measured.

    There is no runtime harness in the toolkit: PresentMon/RenderDoc need a
    *running* application to sample, and the toolkit ships none. So the honest
    answer on a machine without a harness is `measured=False`.
    """
    presentmon = toolbox.discover("presentmon")
    renderdoc = toolbox.discover("renderdoc")
    if not presentmon.available and not renderdoc.available:
        return {
            "frame_ms_p50": None,
            "frame_ms_p95": None,
            "frame_ms_p99": None,
            "fps": None,
            "measured": False,
            "reason": "no frame-time capture tool installed and no runtime harness in the toolkit "
                      f"({toolbox.INSTALL_HINTS['presentmon']}; {toolbox.INSTALL_HINTS['renderdoc']}). "
                      "Frame time is NOT measured and no estimate is reported.",
        }
    return {
        "frame_ms_p50": None,
        "frame_ms_p95": None,
        "frame_ms_p99": None,
        "fps": None,
        "measured": False,
        "reason": "PresentMon/RenderDoc are installed but no scene is running from this toolkit; "
                  "capture the frames in the renderer harness and re-run benchmark_asset with "
                  "the captured report. No frame time is inferred here.",
        "tools_present": [t.to_dict() for t in (presentmon, renderdoc) if t.available],
    }


def vram_estimate(per_lod_resident: dict[str, int], texture_bytes: int) -> dict[str, Any]:
    """Lower-bound VRAM footprint. Explicitly an estimate, never a measurement."""
    geometry = sum(per_lod_resident.values())
    total = geometry + texture_bytes
    return {
        "value": float(total),
        "unit": "bytes",
        "measured": False,
        "method": "sum of live buffer views + texture bytes (lower bound; driver-side "
                  "allocation, alignment and padding are NOT measured)",
        "geometry_bytes": geometry,
        "texture_bytes": texture_bytes,
        "budget_vram_bytes": GTX_1080_BUDGET.vram_gb * (1 << 30),
        "fraction_of_vram": round(total / (GTX_1080_BUDGET.vram_gb * (1 << 30)), 6),
    }


def streaming_cost(per_lod_bytes: dict[str, int]) -> dict[str, Any]:
    """Bytes that must be resident / streamed per LOD. Wall-clock cost is not measured."""
    return {
        "resident_if_all_loaded_bytes": sum(per_lod_bytes.values()),
        "per_lod_bytes": dict(sorted(per_lod_bytes.items())),
        "lod0_bytes": per_lod_bytes.get("lod0", 0),
        "wall_clock_ms": None,
        "measured": False,
        "method": "file bytes per LOD; upload time depends on the host's disk/IO path and is "
                  "NOT measured by the toolkit",
    }


def compare_before_after(before: dict[str, Any], after: dict[str, Any]) -> list[Measurement]:
    """Before/after deltas between a source asset and its optimized artifact.

    Both sides are measured file/geometry facts, so the deltas are measured too.
    """
    out: list[Measurement] = []
    for key in ("triangles", "vertices", "draw_calls"):
        before_value = float((before.get("summary") or {}).get(key, 0))
        after_value = float((after.get("summary") or {}).get(key, 0))
        out.append(Measurement(f"before.{key}", before_value, key, True, "source asset"))
        out.append(Measurement(f"after.{key}", after_value, key, True, "optimized artifact"))
        out.append(Measurement(
            f"delta.{key}", (after_value - before_value) / before_value if before_value else 0.0,
            "ratio", True, "(after - before) / before"))
    before_bytes = float(before.get("file_bytes", 0))
    after_bytes = float(after.get("file_bytes", 0))
    out.append(Measurement("before.file_bytes", before_bytes, "bytes", True, "file size"))
    out.append(Measurement("after.file_bytes", after_bytes, "bytes", True, "file size"))
    out.append(Measurement("delta.file_bytes",
                            (after_bytes - before_bytes) / before_bytes if before_bytes else 0.0,
                            "ratio", True, "(after - before) / before"))
    out.append(Measurement("silhouette_drift_pct", 0.0, "percent", False,
                            "requires a rendered before/after comparison; not measured here"))
    return out


def budget_context() -> dict[str, Any]:
    return {
        "reference": GTX_1080_BUDGET.to_dict(),
        "policy": "geometry/texture budgets are per-asset; the frame budget covers the entire "
                  "scene (hero beast, fauna, foliage, terrain, weather, streaming).",
    }


@dataclass
class BenchmarkReport:
    asset_id: str
    measurements: list[Measurement] = field(default_factory=list)
    lods: list[dict[str, Any]] = field(default_factory=list)
    extras: list[dict[str, Any]] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    frame_time: dict[str, Any] = field(default_factory=dict)
    memory: dict[str, Any] = field(default_factory=dict)

    def add_measurement(self, measurement: Measurement) -> Measurement:
        self.measurements.append(measurement)
        return measurement

    def to_dict(self) -> dict:
        return {
            "asset_id": self.asset_id,
            "budget": budget_context(),
            "measurements": [m.to_dict() for m in self.measurements],
            "lods": self.lods,
            "extras": self.extras,
            "frame_time": self.frame_time,
            "memory": self.memory,
            "warnings": list(self.warnings),
        }


__all__ = [
    "GTX_1080_BUDGET",
    "BenchmarkReport",
    "Measurement",
    "ReferenceBudget",
    "budget_context",
    "compare_before_after",
    "frame_time_measurement",
    "measure_glb",
    "resident_bytes",
    "streaming_cost",
    "vram_estimate",
]