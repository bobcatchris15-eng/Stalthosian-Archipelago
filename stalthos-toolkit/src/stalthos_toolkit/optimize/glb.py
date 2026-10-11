"""glTF 2.0 / GLB reading, writing and structural validation.

Scope: **standard glTF only.** No engine-specific conversion happens here (no
engine has been chosen; see docs/toolkit-architecture.md).

This module is deliberately dependency-free and Blender-free. It reads exactly
what it needs:

* the GLB container (12-byte header + JSON chunk + BIN chunk),
* mesh primitive arrays (POSITION / NORMAL / TEXCOORD_n / JOINTS_0 / WEIGHTS_0),
* the node graph, skins and animations, so rig integrity can be checked.

It also appends buffer views/accessors so a LOD document can be derived from a
source document *without* invalidating the animation samplers, which point at
accessors of the original buffer.

Nothing here validates semantic quality; `checks.py` owns acceptance. This
module answers "what is in the file", never "is it good".
"""

from __future__ import annotations

import copy
import json
import re
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable, Sequence

from ..core.errors import ExternalToolFailed, ToolkitError
from . import tools as toolbox

GLB_MAGIC = 0x46546C67          # 'glTF' little-endian
GLB_VERSION = 2
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942

ARRAY_BUFFER = 34962
ELEMENT_ARRAY_BUFFER = 34963

FLOAT = 5126
BYTE = 5120
UNSIGNED_BYTE = 5121
SHORT = 5122
UNSIGNED_SHORT = 5123
UNSIGNED_INT = 5125

_COMPONENT: dict[int, tuple[str, int]] = {
    BYTE: ("b", 1),
    UNSIGNED_BYTE: ("B", 1),
    SHORT: ("h", 2),
    UNSIGNED_SHORT: ("H", 2),
    UNSIGNED_INT: ("I", 4),
    FLOAT: ("f", 4),
}

_NORMALIZE_DIVISOR: dict[int, float] = {
    BYTE: 127.0,
    UNSIGNED_BYTE: 255.0,
    SHORT: 32767.0,
    UNSIGNED_SHORT: 65535.0,
}

_COMPONENT_COUNT: dict[str, int] = {
    "SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT2": 4, "MAT3": 9, "MAT4": 16,
}

MODE_TRIANGLES = 4
MODE_TRIANGLE_STRIP = 5
MODE_TRIANGLE_FAN = 6


class GlbError(ToolkitError):
    """Malformed or unreadable glTF/GLB input. Maps to exit 2 (usage)."""

    exit_code = 2


@dataclass
class MeshArrays:
    """Plain Python mesh arrays, ready for decimation."""

    positions: list[tuple[float, float, float]]
    indices: list[int]
    normals: list[tuple[float, float, float]] | None = None
    uvs: list[tuple[float, float]] | None = None
    joints: list[tuple[int, int, int, int]] | None = None
    weights: list[tuple[float, float, float, float]] | None = None
    uv_set: int = 0
    # (componentType, normalized) of the attributes as they were in the source,
    # so a derived document can re-encode them faithfully.
    uv_component: int = FLOAT
    uv_normalized: bool = False
    joints_component: int = UNSIGNED_BYTE
    weights_component: int = FLOAT
    weights_normalized: bool = False

    @property
    def vertex_count(self) -> int:
        return len(self.positions)

    @property
    def triangle_count(self) -> int:
        return len(self.indices) // 3

    def to_dict(self) -> dict:
        return {
            "vertices": self.vertex_count,
            "triangles": self.triangle_count,
            "has_normals": self.normals is not None,
            "has_uvs": self.uvs is not None,
            "has_skin": self.joints is not None,
        }


@dataclass
class GlbSummary:
    """Structural counts read straight out of the document."""

    nodes: int = 0
    meshes: int = 0
    primitives: int = 0
    vertices: int = 0
    triangles: int = 0
    materials: int = 0
    textures: int = 0
    images: int = 0
    skins: int = 0
    joints: int = 0
    animations: int = 0
    animation_channels: int = 0
    draw_calls: int = 0            # one per primitive that has a POSITION
    materials_per_primitive: list[int] = field(default_factory=list)

    def to_dict(self) -> dict:
        data = dict(self.__dict__)
        data.pop("materials_per_primitive", None)
        return data


class GlbDocument:
    """An in-memory glTF asset plus its optional binary chunk."""

    def __init__(self, gltf: dict[str, Any], binary: bytes = b"") -> None:
        self.gltf = gltf
        self.binary = bytearray(binary)

    # --- container io -------------------------------------------------------
    @classmethod
    def read(cls, path: str | Path) -> "GlbDocument":
        path = Path(path)
        try:
            data = path.read_bytes()
        except OSError as exc:
            raise GlbError(f"cannot read glTF asset: {path}", detail={"error": str(exc)}) from exc
        return cls.from_bytes(data, origin=str(path))

    @classmethod
    def from_bytes(cls, data: bytes, *, origin: str = "<bytes>") -> "GlbDocument":
        if len(data) < 12:
            raise GlbError(f"{origin}: too short to be a GLB ({len(data)} bytes)")
        magic, version, total = struct.unpack_from("<III", data, 0)
        if magic != GLB_MAGIC:
            # A bare .gltf JSON file is legal input too.
            try:
                gltf = json.loads(data.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                raise GlbError(f"{origin}: not a GLB and not a JSON glTF", detail={"error": str(exc)}) from exc
            if not isinstance(gltf, dict):
                raise GlbError(f"{origin}: glTF root must be an object")
            return cls(gltf, b"")
        if version != GLB_VERSION:
            raise GlbError(f"{origin}: unsupported GLB version {version} (expected 2)")
        if total > len(data):
            raise GlbError(f"{origin}: truncated GLB (header claims {total}, file has {len(data)})")

        offset = 12
        gltf: dict[str, Any] | None = None
        binary = b""
        while offset + 8 <= min(total, len(data)):
            length, kind = struct.unpack_from("<II", data, offset)
            offset += 8
            chunk = data[offset:offset + length]
            offset += length
            if kind == CHUNK_JSON:
                try:
                    gltf = json.loads(chunk.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                    raise GlbError(f"{origin}: malformed glTF JSON chunk", detail={"error": str(exc)}) from exc
            elif kind == CHUNK_BIN:
                binary = chunk
        if not isinstance(gltf, dict):
            raise GlbError(f"{origin}: GLB has no JSON chunk")
        return cls(gltf, binary)

    def copy(self) -> "GlbDocument":
        return GlbDocument(copy.deepcopy(self.gltf), bytes(self.binary))

    def write(self, path: str | Path) -> Path:
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        payload = json.dumps(self.gltf, separators=(",", ":"), sort_keys=False).encode("utf-8")
        pad = (4 - len(payload) % 4) % 4
        payload += b" " * pad
        body = bytes(self.binary)
        body_pad = (4 - len(body) % 4) % 4
        body += b"\0" * body_pad
        total = 12 + 8 + len(payload) + (8 + len(body) if body else 0)
        out = bytearray()
        out += struct.pack("<III", GLB_MAGIC, GLB_VERSION, total)
        out += struct.pack("<II", len(payload), CHUNK_JSON) + payload
        if body:
            out += struct.pack("<II", len(body), CHUNK_BIN) + body
        path.write_bytes(bytes(out))
        return path

    # --- buffer helpers -----------------------------------------------------
    def _buffer_bytes(self, buffer_index: int) -> bytes:
        buffers = self.gltf.get("buffers") or []
        if buffer_index >= len(buffers):
            raise GlbError(f"buffer index {buffer_index} out of range ({len(buffers)} buffers)")
        uri = buffers[buffer_index].get("uri")
        if uri is None:
            # A GLB carries at most ONE uri-less buffer: the BIN chunk. gltfpack -cc
            # emits a *second* uri-less buffer for the EXT_meshopt_compression
            # fallback payload. Returning the BIN chunk for it would silently
            # hand back the wrong bytes, so refuse with a typed error instead.
            if buffer_index != 0:
                raise GlbError(
                    f"buffer {buffer_index} has no uri; a GLB's BIN chunk can only back buffer 0",
                    detail={"buffer": buffers[buffer_index], "buffer_index": buffer_index},
                    hint="re-run the external tool without meshopt compression (-cc) so the "
                         "asset stays a single-buffer GLB this reader can decode")
            return bytes(self.binary)
        if uri.startswith("data:"):
            import base64

            _, _, payload = uri.partition(",")
            return base64.b64decode(payload)
        raise GlbError("external buffer URIs are not supported by this reader; embed the GLB",
                       detail={"uri": uri})

    def _buffer_view_bytes(self, index: int) -> tuple[bytes, int, int]:
        views = self.gltf.get("bufferViews") or []
        if index >= len(views):
            raise GlbError(f"bufferView index {index} out of range")
        view = views[index]
        # Under EXT_meshopt_compression the view's byteOffset/byteLength describe
        # the DECODED fallback layout, not bytes present in the BIN chunk. Slicing
        # the chunk by them yields garbage, so say so rather than misread geometry.
        ext = view.get("extensions") or {}
        if "EXT_meshopt_compression" in ext:
            raise GlbError(
                f"bufferView {index} uses EXT_meshopt_compression, which this reader does not decode",
                detail={"view": view},
                hint="re-run without meshopt compression (-cc) to keep the LOD chain readable")
        raw = self._buffer_bytes(int(view.get("buffer", 0)))
        offset = int(view.get("byteOffset", 0))
        length = int(view.get("byteLength", 0))
        return raw[offset:offset + length], offset, length

    def accessor_values(self, index: int) -> list[tuple[float, ...]]:
        """Decode an accessor into a list of tuples. Normalized ints -> floats."""
        accessors = self.gltf.get("accessors") or []
        if index >= len(accessors):
            raise GlbError(f"accessor index {index} out of range")
        accessor = accessors[index]
        component_type = int(accessor.get("componentType", FLOAT))
        atype = str(accessor.get("type", "SCALAR"))
        count = int(accessor.get("count", 0))
        components = _COMPONENT_COUNT.get(atype)
        if components is None:
            raise GlbError(f"unsupported accessor type {atype!r}")
        if component_type not in _COMPONENT:
            raise GlbError(f"unsupported accessor componentType {component_type}")
        if "bufferView" not in accessor:
            return [tuple([0.0] * components)] * count

        view_index = int(accessor["bufferView"])
        views = self.gltf["bufferViews"]
        view = views[view_index]
        raw, _, _ = self._buffer_view_bytes(view_index)   # already sliced to the view
        stride = int(view.get("byteStride") or 0) or components * _COMPONENT[component_type][1]
        start = int(accessor.get("byteOffset", 0))
        fmt, size = _COMPONENT[component_type]
        divisor = _NORMALIZE_DIVISOR.get(component_type, 1.0) if accessor.get("normalized") else 1.0

        element_size = components * size
        if stride < element_size or start < 0 or start + max(0, count - 1) * stride + element_size > len(raw):
            raise GlbError(
                f"accessor {index} exceeds its bufferView",
                detail={"accessor": accessor, "bufferView": view_index,
                        "bufferView_bytes": len(raw), "stride": stride},
            )

        out: list[tuple[float, ...]] = []
        for i in range(count):
            base = start + i * stride
            values = struct.unpack_from(f"<{components}{fmt}", raw, base)
            if accessor.get("normalized") and component_type in (BYTE, SHORT):
                # Signed normalized values use max(value / positive_max, -1).
                decoded = tuple(max(-1.0, v / divisor) for v in values)
            else:
                decoded = tuple(v / divisor for v in values)
            out.append(decoded)  # type: ignore[arg-type]
        return out

    def append_buffer_view(self, payload: bytes, *, target: int | None = None) -> int:
        """Append raw bytes to the BIN chunk, 4-byte aligned, return bufferView index."""
        pad = (4 - len(self.binary) % 4) % 4
        self.binary.extend(b"\0" * pad)
        offset = len(self.binary)
        self.binary.extend(payload)
        views = self.gltf.setdefault("bufferViews", [])
        view: dict[str, Any] = {"buffer": 0, "byteOffset": offset, "byteLength": len(payload)}
        if target is not None:
            view["target"] = target
        views.append(view)
        return len(views) - 1

    def add_accessor(self, *, component_type: int, atype: str, count: int,
                     buffer_view: int, byte_offset: int = 0, normalized: bool = False,
                     minmax: tuple[Sequence[float], Sequence[float]] | None = None) -> int:
        accessor: dict[str, Any] = {
            "bufferView": buffer_view,
            "byteOffset": byte_offset,
            "componentType": component_type,
            "count": count,
            "type": atype,
        }
        if normalized:
            accessor["normalized"] = True
        if minmax is not None:
            accessor["min"] = [float(v) for v in minmax[0]]
            accessor["max"] = [float(v) for v in minmax[1]]
        self.gltf.setdefault("accessors", []).append(accessor)
        return len(self.gltf["accessors"]) - 1

    def referenced_accessor_indices(self) -> set[int]:
        """Every accessor the document still points at (meshes, skins, animations)."""
        used: set[int] = set()
        for mesh in self.gltf.get("meshes") or []:
            for primitive in mesh.get("primitives") or []:
                used.update(int(v) for v in (primitive.get("attributes") or {}).values())
                for slot in ("indices",):
                    if slot in primitive:
                        used.add(int(primitive[slot]))
                for target in primitive.get("targets") or []:
                    used.update(int(v) for v in target.values())
        for skin in self.gltf.get("skins") or []:
            if "inverseBindMatrices" in skin:
                used.add(int(skin["inverseBindMatrices"]))
        for animation in self.gltf.get("animations") or []:
            for sampler in animation.get("samplers") or []:
                for slot in ("input", "output"):
                    if slot in sampler:
                        used.add(int(sampler[slot]))
        return used

    def compact(self) -> "GlbDocument":
        """Drop unreferenced accessors/bufferViews and rewrite the BIN chunk.

        Derived LOD documents keep the source buffer verbatim so animation
        samplers stay valid, which leaves the source geometry bytes in the file.
        Compaction removes exactly those dead bytes; it never rewrites a live
        buffer view.
        """
        accessors = self.gltf.get("accessors") or []
        views = self.gltf.get("bufferViews") or []
        keep_accessors = sorted(i for i in self.referenced_accessor_indices() if i < len(accessors))

        keep_views: list[int] = []
        for index in keep_accessors:
            view_index = int(accessors[index].get("bufferView", -1))
            if 0 <= view_index < len(views) and view_index not in keep_views:
                keep_views.append(view_index)
        # Embedded images are direct bufferView consumers too; accessors are
        # not their only owners. Keep their payloads and rewrite references
        # alongside accessor bufferViews so compaction cannot alias image bytes
        # with vertex/index data.
        for image in self.gltf.get("images") or []:
            if "bufferView" in image:
                view_index = int(image["bufferView"])
                if 0 <= view_index < len(views) and view_index not in keep_views:
                    keep_views.append(view_index)
        keep_views.sort()

        blob = bytearray()
        view_remap: dict[int, int] = {}
        new_views: list[dict[str, Any]] = []
        for view_index in keep_views:
            view = views[view_index]
            raw, _, _ = self._buffer_view_bytes(view_index)
            pad = (4 - len(blob) % 4) % 4
            blob.extend(b"\0" * pad)
            offset = len(blob)
            blob.extend(raw)
            new_view = {k: v for k, v in view.items() if k not in ("buffer", "byteOffset", "byteLength")}
            new_view["buffer"] = 0
            new_view["byteOffset"] = offset
            new_view["byteLength"] = len(raw)
            view_remap[view_index] = len(new_views)
            new_views.append(new_view)

        accessor_remap: dict[int, int] = {}
        new_accessors: list[dict[str, Any]] = []
        for index in keep_accessors:
            accessor = dict(accessors[index])
            accessor["bufferView"] = view_remap[int(accessor["bufferView"])]
            accessor_remap[index] = len(new_accessors)
            new_accessors.append(accessor)

        gltf = copy.deepcopy(self.gltf)
        gltf["accessors"] = new_accessors
        gltf["bufferViews"] = new_views
        gltf["buffers"] = [{"byteLength": len(blob)}]
        for mesh in gltf.get("meshes") or []:
            for primitive in mesh.get("primitives") or []:
                primitive["attributes"] = {k: accessor_remap[v]
                                           for k, v in (primitive.get("attributes") or {}).items()}
                if "indices" in primitive:
                    primitive["indices"] = accessor_remap[int(primitive["indices"])]
                if primitive.get("targets"):
                    primitive["targets"] = [{k: accessor_remap[v] for k, v in target.items()}
                                            for target in primitive["targets"]]
        for skin in gltf.get("skins") or []:
            if "inverseBindMatrices" in skin:
                skin["inverseBindMatrices"] = accessor_remap[int(skin["inverseBindMatrices"])]
        for animation in gltf.get("animations") or []:
            for sampler in animation.get("samplers") or []:
                for slot in ("input", "output"):
                    if slot in sampler:
                        sampler[slot] = accessor_remap[int(sampler[slot])]
        for image in gltf.get("images") or []:
            if "bufferView" in image:
                image["bufferView"] = view_remap[int(image["bufferView"])]
        return GlbDocument(gltf, bytes(blob))

    # --- meshes -------------------------------------------------------------
    def iter_primitives(self) -> Iterable[tuple[int, int, dict[str, Any]]]:
        for mesh_index, mesh in enumerate(self.gltf.get("meshes") or []):
            for prim_index, primitive in enumerate(mesh.get("primitives") or []):
                yield mesh_index, prim_index, primitive

    def primitive_arrays(self, primitive: dict[str, Any]) -> MeshArrays | None:
        """Decode a TRIANGLES primitive, or None when it is not triangle data."""
        mode = int(primitive.get("mode", MODE_TRIANGLES))
        if mode not in (MODE_TRIANGLES, MODE_TRIANGLE_STRIP, MODE_TRIANGLE_FAN):
            return None
        attributes = primitive.get("attributes") or {}
        if "POSITION" not in attributes:
            return None
        positions = [tuple(v) for v in self.accessor_values(int(attributes["POSITION"]))]  # type: ignore[misc]

        if "indices" in primitive:
            flat = [int(v[0]) for v in self.accessor_values(int(primitive["indices"]))]
        else:
            flat = list(range(len(positions)))
        if mode == MODE_TRIANGLES:
            indices = flat[: len(flat) - (len(flat) % 3)]
        elif mode == MODE_TRIANGLE_STRIP:
            indices = []
            for i in range(len(flat) - 2):
                if i % 2 == 0:
                    indices += [flat[i], flat[i + 1], flat[i + 2]]
                else:
                    indices += [flat[i + 1], flat[i], flat[i + 2]]
        else:
            indices = []
            for i in range(1, len(flat) - 1):
                indices += [flat[0], flat[i], flat[i + 1]]

        accessors = self.gltf.get("accessors") or []

        def accessor_meta(index: int) -> tuple[int, bool]:
            meta = accessors[int(index)] if int(index) < len(accessors) else {}
            return int(meta.get("componentType", FLOAT)), bool(meta.get("normalized"))

        normals = None
        if "NORMAL" in attributes:
            normals = [tuple(v) for v in self.accessor_values(int(attributes["NORMAL"]))]  # type: ignore[misc]
        uv_key = None
        for key in sorted(attributes):
            if key.startswith("TEXCOORD_"):
                uv_key = key
                break
        uvs = None
        uv_component, uv_normalized = FLOAT, False
        if uv_key is not None:
            uvs = [tuple(v[:2]) for v in self.accessor_values(int(attributes[uv_key]))]  # type: ignore[misc]
            uv_component, uv_normalized = accessor_meta(attributes[uv_key])
        joints = weights = None
        joints_component, weights_component, weights_normalized = UNSIGNED_BYTE, FLOAT, False
        if "JOINTS_0" in attributes:
            joints = [tuple(int(c) for c in v[:4]) for v in self.accessor_values(int(attributes["JOINTS_0"]))]  # type: ignore[misc]
            joints_component, _ = accessor_meta(attributes["JOINTS_0"])
        if "WEIGHTS_0" in attributes:
            weights = [tuple(float(c) for c in v[:4]) for v in self.accessor_values(int(attributes["WEIGHTS_0"]))]  # type: ignore[misc]
            weights_component, weights_normalized = accessor_meta(attributes["WEIGHTS_0"])

        return MeshArrays(
            positions=positions,  # type: ignore[arg-type]
            indices=indices,
            normals=normals,
            uvs=uvs,
            joints=joints,
            weights=weights,
            uv_set=int(uv_key.split("_")[1]) if uv_key else 0,
            uv_component=uv_component,
            uv_normalized=uv_normalized,
            joints_component=joints_component,
            weights_component=weights_component,
            weights_normalized=weights_normalized,
        )

    def summary(self) -> GlbSummary:
        summary = GlbSummary(
            nodes=len(self.gltf.get("nodes") or []),
            meshes=len(self.gltf.get("meshes") or []),
            materials=len(self.gltf.get("materials") or []),
            textures=len(self.gltf.get("textures") or []),
            images=len(self.gltf.get("images") or []),
            skins=len(self.gltf.get("skins") or []),
            animations=len(self.gltf.get("animations") or []),
        )
        for skin in self.gltf.get("skins") or []:
            summary.joints += len(skin.get("joints") or [])
        for animation in self.gltf.get("animations") or []:
            summary.animation_channels += len(animation.get("channels") or [])
        for _, _, primitive in self.iter_primitives():
            summary.primitives += 1
            attributes = primitive.get("attributes") or {}
            if "POSITION" not in attributes:
                continue
            summary.draw_calls += 1
            arrays = self.primitive_arrays(primitive)
            if arrays is None:
                continue
            summary.vertices += arrays.vertex_count
            summary.triangles += arrays.triangle_count
            summary.materials_per_primitive.append(1 if "material" in primitive else 0)
        return summary

    def unique_material_count(self) -> int:
        materials: set[int] = set()
        for _, _, primitive in self.iter_primitives():
            if "material" in primitive:
                materials.add(int(primitive["material"]))
        return len(materials)

    def skinned(self) -> bool:
        for _, _, primitive in self.iter_primitives():
            attributes = primitive.get("attributes") or {}
            if "JOINTS_0" in attributes:
                return True
        return bool(self.gltf.get("skins"))

    def animation_names(self) -> list[str]:
        return [str(a.get("name", "")) for a in (self.gltf.get("animations") or [])]


# --- rig / animation integrity ---------------------------------------------
def check_rig_integrity(doc: GlbDocument) -> list[dict[str, str]]:
    """Structural rig and animation checks. Returns one dict per problem found.

    An empty list means the document is *structurally* consistent; it says
    nothing about whether the rig deforms correctly.
    """
    issues: list[dict[str, str]] = []
    nodes = doc.gltf.get("nodes") or []
    node_index = {int(n.get("index", i)): n for i, n in enumerate(nodes)}
    parents: dict[int, int] = {}
    for i, node in enumerate(nodes):
        for child in node.get("children") or []:
            child_index = int(child)
            if child_index in parents:
                issues.append({"kind": "node_multiple_parents",
                               "detail": f"node {child_index} is a child of more than one parent"})
            parents[child_index] = i

    # Cycles: walk up from each node; bounded by node count.
    for start in range(len(nodes)):
        seen: set[int] = set()
        cursor: int | None = start
        while cursor is not None and cursor not in seen:
            seen.add(cursor)
            cursor = parents.get(cursor)
        if cursor is not None:
            issues.append({"kind": "node_cycle", "detail": f"node hierarchy cycles at node {start}"})
            break

    skins = doc.gltf.get("skins") or []
    for skin_index, skin in enumerate(skins):
        joints = [int(j) for j in skin.get("joints") or []]
        if not joints:
            issues.append({"kind": "skin_without_joints", "detail": f"skin {skin_index} declares no joints"})
        for joint in joints:
            if joint not in node_index and joint >= len(nodes):
                issues.append({"kind": "skin_joint_missing",
                               "detail": f"skin {skin_index} references missing node {joint}"})
        skeleton = skin.get("skeleton")
        if skeleton is not None and int(skeleton) not in joints:
            issues.append({"kind": "skin_skeleton_not_joint",
                           "detail": f"skin {skin_index} skeleton {skeleton} is not in its joint list"})

    joint_set = {int(j) for skin in skins for j in skin.get("joints") or []}
    for mesh_index, prim_index, primitive in doc.iter_primitives():
        attributes = primitive.get("attributes") or {}
        if "JOINTS_0" not in attributes:
            continue
        if "WEIGHTS_0" not in attributes:
            issues.append({"kind": "skin_without_weights",
                           "detail": f"mesh {mesh_index}/{prim_index} has JOINTS_0 but no WEIGHTS_0"})
        if not skins:
            issues.append({"kind": "skinned_mesh_without_skin",
                           "detail": f"mesh {mesh_index}/{prim_index} is skinned but no skin is declared"})

    # Skinned nodes must reference a skin, and skin joints must not be instanced twice.
    skin_of_node: dict[int, int] = {}
    for i, node in enumerate(nodes):
        if "skin" in node:
            skin_of_node[i] = int(node["skin"])
    for node_index_value, skin_index in skin_of_node.items():
        if skin_index >= len(skins):
            issues.append({"kind": "node_skin_out_of_range",
                           "detail": f"node {node_index_value} references missing skin {skin_index}"})
    for joint in joint_set:
        if joint in skin_of_node:
            issues.append({"kind": "joint_is_instanced",
                           "detail": f"joint node {joint} is also a skinned mesh instance"})

    accessors = doc.gltf.get("accessors") or []
    for animation_index, animation in enumerate(doc.gltf.get("animations") or []):
        samplers = animation.get("samplers") or []
        if not animation.get("channels"):
            issues.append({"kind": "animation_without_channels",
                           "detail": f"animation {animation_index} has no channels"})
        for channel in animation.get("channels") or []:
            target = channel.get("target") or {}
            node_ref = target.get("node")
            if node_ref is not None and int(node_ref) >= len(nodes):
                issues.append({"kind": "animation_target_missing",
                               "detail": f"animation {animation_index} targets missing node {node_ref}"})
            sampler_index = int(channel.get("sampler", 0))
            if sampler_index >= len(samplers):
                issues.append({"kind": "animation_sampler_missing",
                               "detail": f"animation {animation_index} references sampler {sampler_index}"})
                continue
            sampler = samplers[sampler_index]
            for slot in ("input", "output"):
                accessor_index = int(sampler.get(slot, -1))
                if accessor_index < 0 or accessor_index >= len(accessors):
                    issues.append({"kind": "animation_accessor_missing",
                                   "detail": f"animation {animation_index} sampler {slot} accessor out of range"})
            if int(sampler.get("input", -1)) < len(accessors):
                key_accessor = accessors[int(sampler["input"])]
                if int(key_accessor.get("count", 0)) < 2:
                    issues.append({"kind": "animation_keyframe_count",
                                   "detail": f"animation {animation_index} sampler has fewer than 2 keyframes"})
    return issues


def rig_summary(doc: GlbDocument) -> dict[str, Any]:
    """Facts for the benchmark report; all read from the file, none inferred."""
    issues = check_rig_integrity(doc)
    return {
        "skins": len(doc.gltf.get("skins") or []),
        "joints": sum(len(skin.get("joints") or []) for skin in doc.gltf.get("skins") or []),
        "animations": len(doc.gltf.get("animations") or []),
        "animation_names": doc.animation_names(),
        "skinned": doc.skinned(),
        "structural_issues": len(issues),
        "structural_ok": not issues,
        "issues": issues[:20],
    }


# --- external validation ----------------------------------------------------
def validate_with_tools(path: str | Path, *, timeout: float | None = None) -> dict[str, Any]:
    """Run an external glTF validator when one is installed.

    Returns a dict describing what was attempted. Never raises for an absent
    tool: the caller decides whether "not validated" blocks or warns.
    """
    path = Path(path)
    result: dict[str, Any] = {"validated": False, "tool": None, "reason": "", "issues": []}

    gltfpack = toolbox.discover("gltf_validator")
    if gltfpack.available:
        try:
            proc = toolbox.run_tool(gltfpack.path or "", ["-p", str(path)], timeout=timeout)
        except (OSError, TimeoutError) as exc:
            result["reason"] = f"validator could not be run: {exc}"
            return result
        try:
            payload = json.loads(proc.stdout or "{}")
        except json.JSONDecodeError:
            payload = {"numErrors": 1 if proc.returncode else 0, "raw": (proc.stdout or "")[:2000]}
        messages = (payload.get("issues") or {}).get("messages") or []
        errors = int(payload.get("numErrors", 1 if proc.returncode else 0) or 0)
        if proc.returncode:
            errors = max(1, errors)
        result.update({"validated": True, "tool": "gltf_validator",
                       "errors": errors,
                       "warnings": payload.get("numWarnings", 0),
                       "issues": messages[:20], "returncode": proc.returncode})
        return result

    transform = toolbox.discover("gltf_transform")
    if transform.available:
        try:
            proc = toolbox.run_tool(transform.path or "", ["validate", str(path)], timeout=timeout)
        except (OSError, TimeoutError) as exc:
            result["reason"] = f"gltf-transform could not be run: {exc}"
            return result
        output = "\n".join(part for part in (proc.stdout, proc.stderr) if part)
        plain_output = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", output)
        errors = 1 if proc.returncode else _gltf_transform_error_count(plain_output)
        result.update({"validated": True, "tool": "gltf_transform", "returncode": proc.returncode,
                       "errors": errors, "issues": [plain_output[:2000]]})
        return result

    result["reason"] = "no glTF validator installed (gltf-validator or gltf-transform); structural check only"
    return result


def _gltf_transform_error_count(output: str) -> int:
    """Count severity-zero rows in gltf-transform's validator table.

    The CLI prints an ``ERROR`` section heading even when it found no errors,
    followed by ``info: No errors found.``. Treating that heading as an issue
    made a successful validation look like a failure. Count actual rows from
    the table's severity column instead.
    """
    if re.search(r"\bno\s+errors?\s+found\b", output, re.IGNORECASE):
        return 0

    lines = output.splitlines()
    header_index = None
    severity_index = None
    for index, line in enumerate(lines):
        if not re.search(r"\bcode\b", line, re.IGNORECASE) or not re.search(
            r"\bseverity\b", line, re.IGNORECASE
        ):
            continue
        cells = [cell.strip().lower() for cell in re.split(r"[│|]", line) if cell.strip()]
        if "severity" in cells:
            header_index = index
            severity_index = cells.index("severity")
            break
    if header_index is None or severity_index is None:
        return 0

    errors = 0
    for line in lines[header_index + 1:]:
        if not re.search(r"[│|]", line):
            continue
        cells = [cell.strip() for cell in re.split(r"[│|]", line) if cell.strip()]
        if len(cells) > severity_index and cells[severity_index] == "0":
            errors += 1
    return errors


def validate_structure(doc: GlbDocument) -> dict[str, Any]:
    """Offline structural check: the reader's own contract, no external tool."""
    issues = check_rig_integrity(doc)
    summary = doc.summary()
    if summary.triangles == 0 and summary.draw_calls > 0:
        issues.append({"kind": "no_triangles", "detail": "document has draw calls but zero triangles"})
    return {
        "validated": True,
        "tool": "stalthos.optimize.glb",
        "errors": len(issues),
        "issues": issues[:20],
        "summary": summary.to_dict(),
    }


def run_gltfpack_simplify(source: str | Path, target: str | Path, ratio: float, *,
                          timeout: float | None = None) -> dict[str, Any]:
    """Simplify with gltfpack (`-si`), the meshoptimizer-backed primary path.

    `ratio` is the fraction of triangles to KEEP (0 < ratio <= 1).
    """
    state = toolbox.discover("gltfpack")
    if not state.available:
        raise GlbError("gltfpack is not available",
                       detail={"ratio": ratio},
                       hint=toolbox.INSTALL_HINTS["gltfpack"])
    ratio = max(0.001, min(1.0, float(ratio)))
    # NOTE: no -cc. Meshopt compression makes gltfpack emit a second uri-less
    # buffer and offset-decode views; this toolkit's own GLB writer never produces
    # EXT_meshopt_compression, so a compressed LOD could not be re-read, validated
    # or published. Determinism and round-trippability beat the bytes saved.
    args = ["-i", str(source), "-o", str(target), "-si", f"{ratio:.6f}"]
    try:
        proc = toolbox.run_tool(state.path or "", args, timeout=timeout)
    except TimeoutError as exc:
        raise ExternalToolFailed("gltfpack", -1, stderr=f"timeout: {exc}") from exc
    except OSError as exc:
        raise GlbError(f"gltfpack could not be launched: {exc}") from exc
    if proc.returncode != 0 or not Path(target).exists():
        raise ExternalToolFailed("gltfpack", proc.returncode, stderr=proc.stderr or proc.stdout)
    return {"tool": "gltfpack", "ratio": ratio, "returncode": proc.returncode,
            "version": state.version_or_unknown()}


def external_tool_versions(*, with_version: bool = False) -> dict[str, str]:
    """tool_versions map for provenance: present tools by version, absent ones
    recorded as "not-available" so provenance is never silently incomplete."""
    out: dict[str, str] = {}
    for name in ("gltfpack", "meshoptimizer", "compressonator", "gltf_validator",
                 "gltf_transform", "presentmon"):
        state = toolbox.discover(name, with_version=with_version)
        out[name] = state.version if state.available and state.version else (
            "present" if state.available else "not-available")
    return out


__all__ = [
    "GlbDocument",
    "GlbError",
    "GlbSummary",
    "MeshArrays",
    "check_rig_integrity",
    "external_tool_versions",
    "rig_summary",
    "run_gltfpack_simplify",
    "validate_structure",
    "validate_with_tools",
]
