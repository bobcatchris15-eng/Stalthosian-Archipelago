"""Pure-python mesh decimation (vertex clustering).

This is the **fallback** simplifier used when meshoptimizer/gltfpack is not
installed. It is deterministic and dependency-free, and it is not as good as
meshoptimizer: it clusters vertices onto a grid, so it cannot preserve sharp
features or UV seams better than the cluster resolution. Everything it produces
must therefore be reported as a *degraded* LOD with the quality claims marked
unmeasured (see `lods.py`).

Design notes
------------
* Determinism: the grid resolution is derived from the data, not from an RNG,
  and cluster order is first-encounter order. The same input always yields the
  same output bytes.
* Skinning: JOINTS_0/WEIGHTS_0 are re-averaged per cluster (weights summed per
  joint, top 4 kept, renormalized) so a skinned mesh stays skinnable. The
  induced skinning error is *not* measured here and must be marked unmeasured.
* Triangle budget: after clustering, if the triangle count is still above the
  budget, the smallest-area triangles are dropped in a deterministic order.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from .glb import MeshArrays


@dataclass(frozen=True)
class DecimateResult:
    mesh: MeshArrays
    requested_ratio: float
    achieved_ratio: float
    cluster_count: int
    dropped_triangles: int

    def to_dict(self) -> dict:
        return {
            "requested_ratio": round(self.requested_ratio, 6),
            "achieved_ratio": round(self.achieved_ratio, 6),
            "clusters": self.cluster_count,
            "dropped_triangles": self.dropped_triangles,
        }


def _bounds(positions: list[tuple[float, float, float]]) -> tuple[list[float], list[float]]:
    if not positions:
        return [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]
    mins = [math.inf] * 3
    maxs = [-math.inf] * 3
    for p in positions:
        for axis in range(3):
            if p[axis] < mins[axis]:
                mins[axis] = p[axis]
            if p[axis] > maxs[axis]:
                maxs[axis] = p[axis]
    return mins, maxs


def _cluster_positions(positions: list[tuple[float, float, float]], indices: list[int],
                       divisions: int) -> dict[tuple[int, int, int], list[int]]:
    mins, maxs = _bounds(positions)
    extent = [max(maxs[a] - mins[a], 1e-9) for a in range(3)]
    cells: dict[tuple[int, int, int], list[int]] = {}
    for vertex_index, p in enumerate(positions):
        key = tuple(min(divisions - 1, int((p[a] - mins[a]) / extent[a] * divisions)) for a in range(3))
        cells.setdefault(key, []).append(vertex_index)  # type: ignore[arg-type]
    # Keep only clusters actually referenced by a triangle: unreferenced
    # vertices are a geometry defect, not something a LOD should carry.
    referenced = {i for i in indices}
    return {key: members for key, members in cells.items() if any(i in referenced for i in members)}


def _divisions_for(target_vertices: int, bounds_cells: int) -> int:
    """Choose a grid resolution whose expected vertex count is closest to target.

    Cubic grid: vertices ~ (divisions^3) * fill, and we clamp to [1, 32] because
    beyond 32^3 clustering degenerates into "copy the mesh".
    """
    if target_vertices <= 0:
        return 1
    best, best_error = 1, None
    for divisions in range(1, 33):
        estimate = (divisions ** 3) * bounds_cells
        error = abs(estimate - target_vertices)
        if best_error is None or error < best_error:
            best, best_error = divisions, error
    return best


def recompute_normals(mesh: MeshArrays) -> list[tuple[float, float, float]]:
    """Area-weighted vertex normals from the current triangles."""
    normals = [[0.0, 0.0, 0.0] for _ in mesh.positions]
    p = mesh.positions
    for i in range(0, len(mesh.indices) - 2, 3):
        a, b, c = mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]
        pa, pb, pc = p[a], p[b], p[c]
        u = (pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2])
        v = (pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2])
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        for index in (a, b, c):
            normals[index][0] += n[0]
            normals[index][1] += n[1]
            normals[index][2] += n[2]
    out: list[tuple[float, float, float]] = []
    for n in normals:
        length = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
        out.append((n[0] / length, n[1] / length, n[2] / length) if length > 1e-12 else (0.0, 0.0, 1.0))
    return out


def _triangle_area(mesh: MeshArrays, a: int, b: int, c: int) -> float:
    pa, pb, pc = mesh.positions[a], mesh.positions[b], mesh.positions[c]
    u = (pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2])
    v = (pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2])
    cross = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    return 0.5 * math.sqrt(cross[0] ** 2 + cross[1] ** 2 + cross[2] ** 2)


def _drop_smallest_triangles(mesh: MeshArrays, target_triangles: int) -> tuple[MeshArrays, int]:
    """Deterministically drop the lowest-area triangles until under budget."""
    triangle_count = mesh.triangle_count
    if triangle_count <= target_triangles or triangle_count == 0:
        return mesh, 0
    scored = []
    for t in range(triangle_count):
        a, b, c = mesh.indices[3 * t], mesh.indices[3 * t + 1], mesh.indices[3 * t + 2]
        scored.append((_triangle_area(mesh, a, b, c), t))
    # Largest area first so we can keep the head of a stable sort.
    scored.sort(key=lambda item: (-item[0], item[1]))
    keep = sorted(t for _, t in scored[:max(0, target_triangles)])
    indices: list[int] = []
    for t in keep:
        indices += mesh.indices[3 * t:3 * t + 3]
    dropped = triangle_count - len(keep)
    return _compact(mesh, indices), dropped


def _compact(mesh: MeshArrays, indices: list[int]) -> MeshArrays:
    """Rebuild the mesh around the vertices actually used by `indices`."""
    used: dict[int, int] = {}
    for i in indices:
        if i not in used:
            used[i] = len(used)
    order = sorted(used)          # stable, input-order remap
    positions = [mesh.positions[i] for i in order]
    new_indices = [used[i] for i in indices]
    return MeshArrays(
        positions=positions,
        indices=new_indices,
        normals=[mesh.normals[i] for i in order] if mesh.normals else None,
        uvs=[mesh.uvs[i] for i in order] if mesh.uvs else None,
        joints=[mesh.joints[i] for i in order] if mesh.joints else None,
        weights=[mesh.weights[i] for i in order] if mesh.weights else None,
        uv_set=mesh.uv_set,
        uv_component=mesh.uv_component,
        uv_normalized=mesh.uv_normalized,
        joints_component=mesh.joints_component,
        weights_component=mesh.weights_component,
        weights_normalized=mesh.weights_normalized,
    )


def cluster_decimate(mesh: MeshArrays, ratio: float) -> DecimateResult:
    """Reduce `mesh` to approximately `ratio` of its triangles."""
    ratio = max(0.0, min(1.0, float(ratio)))
    source_triangles = mesh.triangle_count
    if source_triangles == 0 or ratio >= 1.0:
        return DecimateResult(mesh=mesh, requested_ratio=ratio, achieved_ratio=1.0,
                              cluster_count=mesh.vertex_count, dropped_triangles=0)

    target_triangles = max(1, int(math.floor(source_triangles * ratio)))
    target_vertices = max(4, int(math.ceil(target_triangles / 2)))

    # Two passes: cluster, then drop whatever is still over budget. Bounds are
    # recomputed after clustering because the cluster grid changes the extent.
    working = mesh
    dropped = 0
    for _ in range(2):
        cells = _cluster_positions(working.positions, working.indices,
                                   _divisions_for(max(4, target_vertices // 2), 1.0))
        working = _cluster_mesh(working, cells)
        if working.triangle_count > target_triangles:
            working, dropped_now = _drop_smallest_triangles(working, target_triangles)
            dropped += dropped_now
        else:
            break

    achieved = working.triangle_count / source_triangles
    return DecimateResult(mesh=working, requested_ratio=ratio, achieved_ratio=achieved,
                          cluster_count=working.vertex_count, dropped_triangles=dropped)


def _cluster_mesh(mesh: MeshArrays, cells: dict[tuple[int, int, int], list[int]]) -> MeshArrays:
    """Collapse each cluster to one vertex (averaged attributes)."""
    positions: list[tuple[float, float, float]] = []
    remap: dict[int, int] = {}
    for key in sorted(cells):
        members = cells[key]
        count = len(members)
        new_index = len(positions)
        positions.append((
            sum(mesh.positions[i][0] for i in members) / count,
            sum(mesh.positions[i][1] for i in members) / count,
            sum(mesh.positions[i][2] for i in members) / count,
        ))
        for i in members:
            remap[i] = new_index

    indices: list[int] = []
    for i in range(0, len(mesh.indices) - 2, 3):
        a, b, c = (remap.get(mesh.indices[i + k]) for k in range(3))
        if a is None or b is None or c is None or a == b or b == c or a == c:
            continue  # degenerate after collapse
        indices += [a, b, c]

    normals = None
    if mesh.normals:
        normals = []
        for members in cells.values():
            count = len(members)
            normals.append((
                sum(mesh.normals[i][0] for i in members) / count,
                sum(mesh.normals[i][1] for i in members) / count,
                sum(mesh.normals[i][2] for i in members) / count,
            ))

    uvs = None
    if mesh.uvs:
        uvs = []
        for members in cells.values():
            count = len(members)
            uvs.append((
                sum(mesh.uvs[i][0] for i in members) / count,
                sum(mesh.uvs[i][1] for i in members) / count,
            ))

    joints = weights = None
    if mesh.joints and mesh.weights:
        joints = []
        weights = []
        for members in cells.values():
            totals: dict[int, float] = {}
            for i in members:
                for joint, weight in zip(mesh.joints[i], mesh.weights[i]):
                    if weight <= 0.0:
                        continue
                    totals[int(joint)] = totals.get(int(joint), 0.0) + float(weight)
            top = sorted(totals.items(), key=lambda item: (-item[1], item[0]))[:4]
            while len(top) < 4:
                top.append((top[-1][0] if top else 0, 0.0))
            total_weight = sum(w for _, w in top) or 1.0
            joints.append((top[0][0], top[1][0], top[2][0], top[3][0]))
            weights.append((top[0][1] / total_weight, top[1][1] / total_weight,
                            top[2][1] / total_weight, top[3][1] / total_weight))

    clustered = MeshArrays(
        positions=positions, indices=indices, normals=normals, uvs=uvs,
        joints=joints, weights=weights, uv_set=mesh.uv_set,
        uv_component=mesh.uv_component, uv_normalized=mesh.uv_normalized,
        joints_component=mesh.joints_component, weights_component=mesh.weights_component,
        weights_normalized=mesh.weights_normalized,
    )
    if mesh.normals:
        clustered.normals = recompute_normals(clustered)
    return clustered


def collision_proxy(mesh: MeshArrays, *, max_triangles: int = 512) -> MeshArrays:
    """A cheap stand-in for a collision hull.

    Honest scope: this is a decimated shell of the source mesh, NOT a convex
    hull or a convex decomposition. Callers must record
    `collision_convex=False` and mark hull-quality metrics unmeasured.
    """
    if mesh.triangle_count <= max_triangles:
        return mesh
    ratio = max(1, max_triangles) / mesh.triangle_count
    return cluster_decimate(mesh, ratio).mesh


__all__ = ["DecimateResult", "cluster_decimate", "collision_proxy", "recompute_normals"]