"""LOD chain, collision proxy and impostor stub (pipeline stage 6).

Policy (*Proposed*, hardware-target derived)
---------------------------------------------
Target: GTX 1080 8 GB, 30 FPS floor = **33.33 ms/frame** for the whole scene.
Nothing here can prove a frame budget; what it does is hold a *geometry budget*
per asset class that leaves room for the rest of the scene, and report every
number it measured so the budget can be checked against a measured frame time
later (see `benchmark/`).

======  ========  ========  ==================================================
class   LOD0      LOD1/2/3  rationale
======  ========  ========  ==================================================
plant   100 %     40/15/5 %  dense foliage silhouettes: three distance steps
creature 100 %   50/20/8 %  creatures carry rig + animation; more geometry kept
                      at LOD1 because it is seen in motion up close
======  ========  ========  ==================================================

Collision proxy: <= 512 triangles, decimated shell (NOT a convex hull — a hull
is not implemented and the hull-quality metrics are recorded as unmeasured).
Impostor: a **stub descriptor only**. No impostor bake is implemented; the file
records the parameters such a bake would need, and every impostor metric is
marked `measured=False`.

Simplifier selection
--------------------
1. ``gltfpack`` (meshoptimizer) — primary, `-si <ratio>`.
2. ``optimize.decimate`` — pure-python fallback, degraded.

A degraded chain always warns and marks the affected metrics `measured=False`.
An LOD is never described as optimized when no optimizer ran.
"""

from __future__ import annotations

import json
import struct
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Sequence

from ..core.seeding import sha256_file
from . import tools as toolbox
from .decimate import DecimateResult, cluster_decimate, collision_proxy
from .glb import (
    ARRAY_BUFFER,
    ELEMENT_ARRAY_BUFFER,
    FLOAT,
    UNSIGNED_BYTE,
    UNSIGNED_INT,
    UNSIGNED_SHORT,
    GlbDocument,
    GlbError,
    MeshArrays,
    check_rig_integrity,
    run_gltfpack_simplify,
    validate_with_tools,
)

COLLISION_MAX_TRIANGLES = 512
IMPOSTOR_VIEWS = 8
IMPOSTOR_RESOLUTION = 256


@dataclass(frozen=True)
class LodPolicy:
    """Triangle retention ratios and fixed budgets for one asset class."""

    name: str
    ratios: tuple[float, ...]
    collision_triangles: int = COLLISION_MAX_TRIANGLES
    impostor_views: int = IMPOSTOR_VIEWS
    impostor_resolution: int = IMPOSTOR_RESOLUTION

    @property
    def levels(self) -> int:
        return len(self.ratios)


PLANT_POLICY = LodPolicy("plant", (1.0, 0.40, 0.15, 0.05))
CREATURE_POLICY = LodPolicy("creature", (1.0, 0.50, 0.20, 0.08))
PROP_POLICY = LodPolicy("prop", (1.0, 0.45, 0.18, 0.07))
SCENE_POLICY = LodPolicy("scene", (1.0, 0.60, 0.30, 0.15))

POLICIES: dict[str, LodPolicy] = {
    "plant": PLANT_POLICY,
    "creature": CREATURE_POLICY,
    "prop": PROP_POLICY,
    "scene": SCENE_POLICY,
}


def policy_for(kind: str | None, *, levels: int | None = None) -> LodPolicy:
    """Policy for an asset kind; an explicit level count truncates/extends it."""
    base = POLICIES.get(str(kind or "").lower(), PLANT_POLICY)
    if levels and levels != len(base.ratios):
        ratios = list(base.ratios)
        while len(ratios) < levels:
            ratios.append(max(0.01, round(ratios[-1] * 0.4, 4)))
        base = LodPolicy(base.name, tuple(ratios[:levels]), base.collision_triangles,
                         base.impostor_views, base.impostor_resolution)
    return base


@dataclass
class LodRecord:
    """One generated level. Every number here is counted from the written file."""

    index: int
    name: str
    path: str
    triangles: int
    vertices: int
    materials: int
    textures: int
    draw_calls: int
    bytes: int
    sha256: str
    target_ratio: float
    simplification_ratio: float
    simplifier: str
    degraded: bool
    measured: dict[str, bool] = field(default_factory=dict)

    def to_dict(self) -> dict:
        data = asdict(self)
        data["simplification_ratio"] = round(self.simplification_ratio, 6)
        data["bytes"] = int(self.bytes)
        return data

    def importer_settings(self) -> dict[str, Any]:
        """What `validate/checks.py` reads back: `lod` + `triangles`."""
        return {
            "lod": self.index,
            "triangles": self.triangles,
            "vertices": self.vertices,
            "materials": self.materials,
            "textures": self.textures,
            "simplifier": self.simplifier,
            "degraded": self.degraded,
            "generated_by": "generate_lods",
        }


@dataclass
class ChainResult:
    records: list[LodRecord] = field(default_factory=list)
    collision: dict[str, Any] | None = None
    impostor: dict[str, Any] | None = None
    policy: str = PLANT_POLICY.name
    simplifier: str = ""
    degraded: bool = False
    warnings: list[str] = field(default_factory=list)
    tool_versions: dict[str, str] = field(default_factory=dict)
    validation: dict[str, Any] = field(default_factory=dict)
    seed: int = 0

    @property
    def triangles(self) -> list[int]:
        return [r.triangles for r in self.records]

    def monotonic(self) -> bool:
        counts = self.triangles
        return all(b < a for a, b in zip(counts, counts[1:]))


# --- attribute re-encoding --------------------------------------------------
def _pack_component(component_type: int, normalized: bool, value: float) -> float:
    """Encode one [0,1] (or [-1,1]) value into an integer component.

    Returns a float so the FLOAT branch can pass the value through unchanged.
    """
    if component_type == FLOAT:
        return float(value)
    if normalized:
        if component_type == 5120:      # BYTE
            return float(max(-128, min(127, int(round(value * 127.0)))))
        if component_type == UNSIGNED_SHORT:
            return float(max(0, min(65535, int(round(value * 65535.0)))))
        return float(max(0, min(255, int(round(max(0.0, min(1.0, value)) * 255.0)))))
    limit = 65535.0 if component_type == UNSIGNED_SHORT else 255.0
    return float(max(0, min(int(limit), int(round(max(0.0, min(1.0, value)) * limit)))))


def _uv_bytes(mesh: MeshArrays, uvs: Sequence[tuple[float, float]]) -> bytes:
    out = bytearray()
    for u, v in uvs:
        if mesh.uv_component == FLOAT:
            out += struct.pack("<2f", float(u), float(v))
        elif mesh.uv_component == UNSIGNED_SHORT:
            out += struct.pack("<2H", _pack_component(UNSIGNED_SHORT, mesh.uv_normalized, u),
                               _pack_component(UNSIGNED_SHORT, mesh.uv_normalized, v))
        else:
            out += struct.pack("<2B", _pack_component(UNSIGNED_BYTE, True, u),
                               _pack_component(UNSIGNED_BYTE, True, v))
    return bytes(out)


def _weights_bytes(mesh: MeshArrays, weights: Sequence[tuple[float, float, float, float]]) -> bytes:
    out = bytearray()
    for w in weights:
        if mesh.weights_component == FLOAT:
            out += struct.pack("<4f", *w)
        elif mesh.weights_component == UNSIGNED_SHORT:
            out += struct.pack("<4H", *[_pack_component(UNSIGNED_SHORT, mesh.weights_normalized, x)
                                      for x in w])
        else:
            out += struct.pack("<4B", *[_pack_component(UNSIGNED_BYTE, True, x) for x in w])
    return bytes(out)


def _apply_arrays(doc: GlbDocument, primitive: dict[str, Any], mesh: MeshArrays) -> dict[str, Any]:
    """Append `mesh` to the document's BIN chunk and return a new primitive dict.

    The original buffer is preserved byte-for-byte, so skins, animations and
    every accessor they reference stay valid. New accessors are appended.
    """
    attributes: dict[str, Any] = {}

    positions = bytearray()
    mins = [float("inf")] * 3
    maxs = [float("-inf")] * 3
    for p in mesh.positions:
        positions += struct.pack("<3f", float(p[0]), float(p[1]), float(p[2]))
        for axis in range(3):
            mins[axis] = min(mins[axis], p[axis])
            maxs[axis] = max(maxs[axis], p[axis])
    if mesh.positions:
        view = doc.append_buffer_view(bytes(positions), target=ARRAY_BUFFER)
        attributes["POSITION"] = doc.add_accessor(
            component_type=FLOAT, atype="VEC3", count=len(mesh.positions), buffer_view=view,
            minmax=([0.0 if mins[0] == float("inf") else mins[0],
                     0.0 if mins[1] == float("inf") else mins[1],
                     0.0 if mins[2] == float("inf") else mins[2]],
                    [0.0 if maxs[0] == float("-inf") else maxs[0],
                     0.0 if maxs[1] == float("-inf") else maxs[1],
                     0.0 if maxs[2] == float("-inf") else maxs[2]]),
        )

    if mesh.normals:
        payload = bytearray()
        for n in mesh.normals:
            payload += struct.pack("<3f", float(n[0]), float(n[1]), float(n[2]))
        view = doc.append_buffer_view(bytes(payload), target=ARRAY_BUFFER)
        attributes["NORMAL"] = doc.add_accessor(component_type=FLOAT, atype="VEC3",
                                                 count=len(mesh.normals), buffer_view=view)

    if mesh.uvs:
        payload = _uv_bytes(mesh, mesh.uvs)
        view = doc.append_buffer_view(payload, target=ARRAY_BUFFER)
        attributes[f"TEXCOORD_{mesh.uv_set}"] = doc.add_accessor(
            component_type=mesh.uv_component, atype="VEC2", count=len(mesh.uvs),
            buffer_view=view, normalized=mesh.uv_normalized)

    if mesh.joints and mesh.weights:
        joint_payload = bytearray()
        for j in mesh.joints:
            joint_payload += struct.pack("<4B", *[max(0, min(255, int(c))) for c in j])
        view = doc.append_buffer_view(bytes(joint_payload), target=ARRAY_BUFFER)
        attributes["JOINTS_0"] = doc.add_accessor(component_type=UNSIGNED_BYTE, atype="VEC4",
                                                  count=len(mesh.joints), buffer_view=view)
        weight_payload = _weights_bytes(mesh, mesh.weights)
        view = doc.append_buffer_view(weight_payload, target=ARRAY_BUFFER)
        attributes["WEIGHTS_0"] = doc.add_accessor(
            component_type=mesh.weights_component, atype="VEC4", count=len(mesh.weights),
            buffer_view=view, normalized=mesh.weights_normalized)

    if mesh.indices:
        index_payload = bytearray()
        for i in mesh.indices:
            index_payload += struct.pack("<I", int(i))
        view = doc.append_buffer_view(bytes(index_payload), target=ELEMENT_ARRAY_BUFFER)
        attributes_index = doc.add_accessor(component_type=UNSIGNED_INT, atype="SCALAR",
                                            count=len(mesh.indices), buffer_view=view)
    else:
        attributes_index = None

    new_primitive: dict[str, Any] = {
        "attributes": attributes,
        "mode": 4,
    }
    for key in ("material", "name", "targets", "extras"):
        if key in primitive:
            new_primitive[key] = primitive[key]
    if attributes_index is not None:
        new_primitive["indices"] = attributes_index
    return new_primitive


def _decimate_document(source: GlbDocument, ratio: float) -> tuple[GlbDocument, list[DecimateResult]]:
    """Pure-python LOD: clone the document and decimate every triangle primitive."""
    doc = source.copy()
    results: list[DecimateResult] = []
    meshes = doc.gltf.get("meshes") or []
    for mesh in meshes:
        primitives = mesh.get("primitives") or []
        for index, primitive in enumerate(primitives):
            arrays = doc.primitive_arrays(primitive)
            if arrays is None:
                continue
            outcome = cluster_decimate(arrays, ratio)
            results.append(outcome)
            mesh["primitives"][index] = _apply_arrays(doc, primitive, outcome.mesh)
    return doc.compact(), results


def _trim_document(source: GlbDocument, target_triangles: int) -> tuple[GlbDocument, int]:
    """Hard-cap a derived document's triangle count (deterministic trim)."""
    doc = source.copy()
    per_primitive = max(1, target_triangles) if target_triangles else 1
    for mesh in doc.gltf.get("meshes") or []:
        primitives = mesh.get("primitives") or []
        for index, primitive in enumerate(primitives):
            arrays = doc.primitive_arrays(primitive)
            if arrays is None or arrays.triangle_count <= per_primitive:
                continue
            ratio = per_primitive / arrays.triangle_count
            outcome = cluster_decimate(arrays, ratio)
            mesh["primitives"][index] = _apply_arrays(doc, primitive, outcome.mesh)
    return doc.compact(), per_primitive


def _count(doc: GlbDocument) -> dict[str, int]:
    summary = doc.summary()
    return {
        "triangles": summary.triangles,
        "vertices": summary.vertices,
        "materials": doc.unique_material_count(),
        "textures": summary.textures,
        "images": summary.images,
        "draw_calls": summary.draw_calls,
        "primitives": summary.primitives,
        "skins": summary.skins,
        "animations": summary.animations,
    }


def _record(index: int, name: str, path: Path, doc: GlbDocument, *, target_ratio: float,
            base_triangles: int, simplifier: str, degraded: bool,
            extra_measured: dict[str, bool] | None = None) -> LodRecord:
    counts = _count(doc)
    return LodRecord(
        index=index,
        name=name,
        path=str(path),
        triangles=counts["triangles"],
        vertices=counts["vertices"],
        materials=counts["materials"],
        textures=counts["textures"],
        draw_calls=counts["draw_calls"],
        bytes=path.stat().st_size,
        sha256=sha256_file(path),
        target_ratio=target_ratio,
        simplification_ratio=(counts["triangles"] / base_triangles) if base_triangles else 0.0,
        simplifier=simplifier,
        degraded=degraded,
        measured={"triangles": True, "vertices": True, "materials": True, "textures": True,
                  "silhouette_drift": False, "gpu_cost": False, **(extra_measured or {})},
    )


# --- public API -------------------------------------------------------------
def build_lod_chain(source_path: str | Path, out_dir: str | Path, *, asset_id: str,
                    policy: LodPolicy = PLANT_POLICY, prefer: str = "auto",
                    allow_degraded: bool = False, timeout: float | None = None,
                    seed: int = 0) -> ChainResult:
    """Build LOD0..LODn, a collision proxy and an impostor stub.

    `prefer`: "auto" | "gltfpack" | "python".
    `allow_degraded`: required to accept the pure-python simplifier.
    """
    source_path = Path(source_path)
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    source = GlbDocument.read(source_path)
    source_counts = _count(source)
    base_triangles = source_counts["triangles"]
    result = ChainResult(policy=policy.name, seed=seed)

    if base_triangles == 0:
        raise GlbError(f"source asset has no triangle geometry: {source_path}",
                       hint="run the generation stage before generate_lods")

    gltfpack = toolbox.discover("gltfpack")
    use_gltfpack = prefer in ("auto", "gltfpack") and gltfpack.available
    if prefer == "gltfpack" and not gltfpack.available:
        raise GlbError("gltfpack requested but not available", hint=toolbox.INSTALL_HINTS["gltfpack"])

    if use_gltfpack:
        result.simplifier = "gltfpack"
    else:
        if prefer == "gltfpack":
            result.simplifier = "python"
        else:
            result.simplifier = "gltfpack" if gltfpack.available else "python"
        result.degraded = True
        if not gltfpack.available:
            result.warnings.append(
                "gltfpack/meshoptimizer not found: LOD chain built with the pure-python "
                "clustering decimator; simplification quality is NOT verified "
                f"({toolbox.INSTALL_HINTS['gltfpack']})")
        if not allow_degraded:
            result.warnings.append(
                "degraded LOD chain was accepted without an explicit opt-in; quality metrics "
                "for this chain stay marked measured=False. No optimizer ran on this geometry.")

    result.tool_versions = {"gltfpack": gltfpack.version or ("present" if gltfpack.available else "not-available"),
                            "simplifier": result.simplifier}

    previous_triangles: int | None = None
    for index, ratio in enumerate(policy.ratios):
        name = f"lod{index}"
        target_path = out_dir / f"{name}.glb"
        target_triangles = max(1, int(base_triangles * ratio))
        if previous_triangles is not None:
            target_triangles = min(target_triangles, previous_triangles - 1)
        if target_triangles < 1:
            raise GlbError(
                f"cannot build {name}: triangle budget exhausted "
                f"(LOD{index - 1} already has {previous_triangles} triangles)",
                hint="reduce the LOD level count for this asset")

        if use_gltfpack and index > 0:
            actual_ratio = target_triangles / base_triangles
            run_gltfpack_simplify(source_path, target_path, actual_ratio, timeout=timeout)
            doc = GlbDocument.read(target_path)
            counts = _count(doc)
            if counts["triangles"] >= (previous_triangles or float("inf")) or \
                    counts["triangles"] > target_triangles:
                # meshoptimizer overshot: enforce the hard cap locally.
                trimmed, _ = _trim_document(doc, target_triangles)
                trimmed.write(target_path)
                doc = trimmed
        else:
            doc, _ = _decimate_document(source, target_triangles / base_triangles) if index > 0 \
                else (source.copy(), [])
            doc.write(target_path)

        # Per-level simplifier truth. LOD0 is a verbatim re-encode of the source
        # geometry -- no optimizer ever touches it -- so it must never be
        # reported as having been produced by gltfpack.
        if index == 0:
            level_simplifier = "source_copy"
        else:
            level_simplifier = "gltfpack" if use_gltfpack else "python"

        record = _record(index, name, target_path, doc, target_ratio=ratio,
                         base_triangles=base_triangles, simplifier=level_simplifier,
                         degraded=result.degraded or level_simplifier != "gltfpack")
        result.records.append(record)
        previous_triangles = record.triangles

    result.collision = _write_collision(source, out_dir, asset_id, policy)
    result.impostor = _write_impostor_stub(out_dir, asset_id, source, result.records[-1])
    result.validation = _validate_chain(result, timeout=timeout)
    return result


def _write_collision(source: GlbDocument, out_dir: Path, asset_id: str,
                     policy: LodPolicy) -> dict[str, Any]:
    """Collision proxy: a decimated shell, explicitly NOT a convex hull."""
    path = out_dir / "collision.glb"
    doc = source.copy()
    for mesh in doc.gltf.get("meshes") or []:
        for index, primitive in enumerate(mesh.get("primitives") or []):
            arrays = doc.primitive_arrays(primitive)
            if arrays is None:
                continue
            proxy = collision_proxy(arrays, max_triangles=policy.collision_triangles)
            mesh["primitives"][index] = _apply_arrays(doc, primitive, proxy)
    # A collision proxy has no business driving skinning or animation.
    doc.gltf.pop("skins", None)
    doc.gltf.pop("animations", None)
    for node in doc.gltf.get("nodes") or []:
        node.pop("skin", None)
    doc = doc.compact()
    doc.write(path)
    counts = _count(GlbDocument.read(path))
    return {
        "path": str(path),
        "sha256": sha256_file(path),
        "bytes": path.stat().st_size,
        "triangles": counts["triangles"],
        "vertices": counts["vertices"],
        "max_triangles": policy.collision_triangles,
        "convex": False,
        "convex_hull_implemented": False,
        "measured": {"triangles": True, "vertices": True, "convex_hull_volume": False,
                     "containment_error": False},
        "note": "decimated shell of the source mesh; convex hull / decomposition NOT implemented",
    }


def _write_impostor_stub(out_dir: Path, asset_id: str, source: GlbDocument,
                        lowest: LodRecord) -> dict[str, Any]:
    """Write the impostor *stub descriptor*. No impostor bake is implemented."""
    path = out_dir / "impostor.json"
    payload = {
        "asset_id": asset_id,
        "status": "stub_not_generated",
        "implemented": False,
        "source_lod": lowest.path,
        "capture": {
            "views": IMPOSTOR_VIEWS,
            "resolution": IMPOSTOR_RESOLUTION,
            "octahedral": True,
            "alpha_silhouette": True,
        },
        "replacement_rule": "camera-facing billboard when screen coverage < 1.5% of viewport height",
        "notes": "No impostor atlas is generated by the toolkit; this file records the "
                 "parameters a future bake would use so the gap is explicit.",
        "metrics": {
            "impostor_atlas_bytes": {"value": 0, "unit": "bytes", "measured": False},
            "impostor_draw_call_cost": {"value": 0, "unit": "calls", "measured": False},
        },
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return {
        "path": str(path),
        "sha256": sha256_file(path),
        "bytes": path.stat().st_size,
        "status": payload["status"],
        "implemented": False,
        "measured": {"impostor_atlas_bytes": False, "impostor_draw_call_cost": False},
    }


def _validate_chain(result: ChainResult, *, timeout: float | None = None) -> dict[str, Any]:
    """Structural self-check of the chain plus external validation if available."""
    issues: list[dict[str, str]] = []
    if not result.monotonic():
        issues.append({"kind": "non_monotonic_lods",
                       "detail": f"triangle counts are not strictly decreasing: {result.triangles}"})
    for record in result.records:
        doc = GlbDocument.read(record.path)
        issues.extend(check_rig_integrity(doc))
        for issue in issues[-5:]:
            issue.setdefault("lod", record.name)
    external = None
    if result.records:
        external = validate_with_tools(result.records[-1].path, timeout=timeout)
        result.tool_versions["gltf_validator"] = str(external.get("tool") or "not-available")
    return {
        "structural_issues": issues,
        "structural_ok": not issues,
        "external": external,
    }


def chain_metrics(result: ChainResult) -> list[dict[str, Any]]:
    """Manifest `metrics` entries for the chain.

    `measured=False` marks anything this stage could not actually measure:
    silhouette drift (needs a render), GPU cost (needs a frame capture), and
    simplification quality on a degraded chain.
    """
    metrics: list[dict[str, Any]] = []

    def add(name: str, value: float, unit: str, measured: bool) -> None:
        metrics.append({"name": name, "value": float(value), "unit": unit, "measured": measured})

    for record in result.records:
        add(f"lod_triangles.{record.name}", record.triangles, "triangles", True)
        add(f"lod_vertices.{record.name}", record.vertices, "vertices", True)
        add(f"lod_materials.{record.name}", record.materials, "materials", True)
        add(f"lod_textures.{record.name}", record.textures, "textures", True)
        add(f"lod_bytes.{record.name}", record.bytes, "bytes", True)
        add(f"lod_simplification_ratio.{record.name}", record.simplification_ratio, "ratio", True)
        # Quality of the simplification is never measured by this stage.
        add(f"lod_silhouette_drift.{record.name}", 0.0, "percent", False)
        # Whether an optimizer actually ran on this level: not measured as a
        # boolean by any downstream tool, and False for a source copy.
        if record.simplifier != "gltfpack":
            add(f"lod_optimizer_applied.{record.name}", 0.0, "boolean", False)

    add("lod_count", len(result.records), "levels", True)
    if result.records:
        add("lod_chain_bytes", sum(r.bytes for r in result.records), "bytes", True)
    if result.collision:
        add("collision_triangles", result.collision["triangles"], "triangles", True)
        add("collision_convex", 0.0, "boolean", False)
    if result.impostor:
        add("impostor_generated", 0.0, "boolean", False)
    return metrics


__all__ = [
    "COLLISION_MAX_TRIANGLES",
    "ChainResult",
    "CREATURE_POLICY",
    "LodPolicy",
    "LodRecord",
    "PLANT_POLICY",
    "POLICIES",
    "PROP_POLICY",
    "build_lod_chain",
    "chain_metrics",
    "policy_for",
]