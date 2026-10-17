"""Conservative audit / seam-preparation for the Smalltrotter source scene.

Run with Blender: blender -b --disable-autoexec --python this_script -- [options]
The audit mode never saves or edits the opened source file. Seam mode saves a
separate derivative after proving a deterministic, unambiguous seam map.
"""
import argparse
import hashlib
import json
import math
import os
import sys
import traceback

try:
    import bpy
    import bmesh
except ImportError:
    bpy = None
    bmesh = None

PLANE_EPS = 1.0e-4
PAIR_EPS = 1.0e-4
GAP_EPS = 1.0e-6
EXIT_OK = 0
EXIT_USAGE = 2
EXIT_MISSING_BPY = 4
EXIT_FAILED = 5


def fail(message):
    raise ValueError(message)


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def resolved(path):
    return os.path.normcase(os.path.realpath(os.path.abspath(path)))


def paths_alias(first, second):
    """Recognize spelling, symlink and existing hardlink aliases."""
    if resolved(first) == resolved(second):
        return True
    try:
        return os.path.samefile(first, second)
    except (FileNotFoundError, OSError):
        return False


def receipt_path_error(source, output, receipt):
    """Reject receipt writes that could replace a source or derivative."""
    if not receipt:
        return None
    if paths_alias(receipt, source):
        return "Receipt aliases the source path; source is read-only"
    if output and paths_alias(receipt, output):
        return "Receipt aliases the derivative output path"
    return None


def bounds(coords):
    if not coords:
        return None
    return {"min": [min(p[i] for p in coords) for i in range(3)],
            "max": [max(p[i] for p in coords) for i in range(3)]}


def topology(mesh):
    """Return counts, connected components, and boundary/nonmanifold edges."""
    if any(not all(math.isfinite(float(c)) for c in v.co) for v in mesh.vertices):
        fail("mesh contains non-finite coordinates")
    edges = {tuple(sorted(e.vertices)): 0 for e in mesh.edges}
    adjacency = [set() for _ in mesh.vertices]
    for poly in mesh.polygons:
        vs = tuple(poly.vertices)
        for a, b in zip(vs, vs[1:] + vs[:1]):
            key = (a, b) if a < b else (b, a)
            edges[key] = edges.get(key, 0) + 1
            adjacency[a].add(b)
            adjacency[b].add(a)
    unseen = set(range(len(mesh.vertices)))
    component_sizes = []
    while unseen:
        root = min(unseen)
        unseen.remove(root)
        stack = [root]
        size = 0
        while stack:
            v = stack.pop()
            size += 1
            for n in sorted(adjacency[v], reverse=True):
                if n in unseen:
                    unseen.remove(n)
                    stack.append(n)
        component_sizes.append(size)
    component_sizes.sort(reverse=True)
    component_size_histogram = {}
    for size in component_sizes:
        component_size_histogram[str(size)] = component_size_histogram.get(str(size), 0) + 1
    return {"vertices": len(mesh.vertices), "edges": len(mesh.edges),
            "faces": len(mesh.polygons), "components": len(component_sizes),
            "component_size_histogram": dict(sorted(component_size_histogram.items(), key=lambda item: int(item[0]), reverse=True)),
            "boundary_edges": sum(1 for count in edges.values() if count == 1),
            "nonmanifold_edges": sum(1 for count in edges.values() if count > 2),
            "bounds": bounds([tuple(v.co) for v in mesh.vertices])}


def mesh_fingerprints(mesh):
    return {"coordinate_index_sha256": hashlib.sha256(b"".join(
                ("%.9g,%.9g,%.9g;" % tuple(v.co)).encode("ascii") for v in mesh.vertices)).hexdigest(),
            "polygon_index_sha256": hashlib.sha256(b"".join(
                (",".join(map(str, p.vertices)) + ";").encode("ascii") for p in mesh.polygons)).hexdigest()}


def seam_pairs(coords, boundary_indices, component_ids):
    """Pair opposite-side, near-plane boundary vertices across components.

    Uses deterministic mutual-unique-nearest matching.
    Vertices must lie within PLANE_EPS of X=0 and have opposite or zero X signs.
    Candidates across components within PAIR_EPS are evaluated:
    - If multiple candidates exist for a vertex, the nearest neighbor distance
      d1 must be strictly smaller than the second-nearest distance d2 by at
      least GAP_EPS (>= 1.0e-6 m). If d2 - d1 < GAP_EPS, the vertex fails closed
      as ambiguous (equal-distance or near-equal ambiguity).
    - Pairing must be mutually unique: a's unique nearest neighbor must be b,
      and b's unique nearest neighbor must be a.
    - If a vertex has candidate counterparts within PAIR_EPS but suffers a
      mutual mismatch (its nearest neighbor b prefers another vertex c, or b
      is ambiguous), the vertex fails closed and is reported in ambiguous.
    """
    candidates = []
    for idx in sorted(set(boundary_indices)):
        p = tuple(float(x) for x in coords[idx])
        if not all(math.isfinite(x) for x in p):
            fail("non-finite source coordinate")
        if abs(p[0]) <= PLANE_EPS:
            candidates.append((idx, p))

    opposing_hits = {idx: [] for idx, _ in candidates}
    for i, (a, pa) in enumerate(candidates):
        for b, pb in candidates[i + 1:]:
            if component_ids[a] == component_ids[b]:
                continue
            if pa[0] * pb[0] > 0.0:
                continue
            d2 = sum((pa[k] - pb[k]) ** 2 for k in range(3))
            if d2 <= PAIR_EPS * PAIR_EPS:
                d = math.sqrt(d2)
                opposing_hits[a].append((d, b))
                opposing_hits[b].append((d, a))

    best_for = {}
    ambiguous = set()

    for idx, hits in opposing_hits.items():
        if not hits:
            continue
        hits.sort(key=lambda item: (item[0], item[1]))
        if len(hits) == 1:
            best_for[idx] = hits[0][1]
        else:
            d1, b1 = hits[0]
            d2, b2 = hits[1]
            if d2 - d1 >= GAP_EPS:
                best_for[idx] = b1
            else:
                ambiguous.add(idx)

    pairs = set()
    for a, b in best_for.items():
        if b is not None:
            if best_for.get(b) == a:
                pairs.add((min(a, b), max(a, b)))
            else:
                ambiguous.add(a)

    sorted_pairs = sorted(pairs)
    sorted_ambiguous = sorted(ambiguous)

    used = set()
    for a, b in sorted_pairs:
        if a in used or b in used:
            fail("seam candidate is paired more than once")
        used.update((a, b))

    return sorted_pairs, len(candidates), sorted_ambiguous


def collect_body(body):
    mesh = body.data
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.verts.ensure_lookup_table()
    bm.edges.ensure_lookup_table()
    bm.faces.ensure_lookup_table()
    # Components over source mesh edges, deterministic traversal.
    unseen = set(v.index for v in bm.verts)
    comp = {}
    comp_id = 0
    while unseen:
        root = min(unseen)
        unseen.remove(root)
        stack = [root]
        while stack:
            vi = stack.pop()
            comp[vi] = comp_id
            v = bm.verts[vi]
            for edge in sorted(v.link_edges, key=lambda e: e.index, reverse=True):
                other = edge.other_vert(v).index
                if other in unseen:
                    unseen.remove(other)
                    stack.append(other)
        comp_id += 1
    boundary = sorted({v.index for e in bm.edges if e.is_boundary for v in e.verts})
    coords = [tuple(v.co) for v in bm.verts]
    pairs, plane_candidate_count, ambiguous = seam_pairs(coords, boundary, comp)
    bm.free()
    return {"topology": topology(mesh), "boundary_vertex_count": len(boundary),
            "near_plane_boundary_candidate_count": plane_candidate_count,
            "candidate_pair_count": len(pairs), "candidate_pairs": [list(p) for p in pairs],
            "ambiguous_candidate_vertex_count": len(ambiguous),
            "ambiguous_candidate_indices_sample": ambiguous[:32],
            "component_ids": comp}


def run_selftest():
    tests = []
    def check(name, condition):
        if not condition:
            raise RuntimeError("selftest failed: " + name)
        tests.append(name)

    # 1. Exact coincident with near neighbor on opposing component (resolved by GAP_EPS)
    coords_nn = [(0.0, 1.0, 1.0), (0.0, 1.0, 1.0), (0.0, 1.00002, 1.0), (0.0, 1.00002, 1.0)]
    p_nn, _, amb_nn = seam_pairs(coords_nn, [0, 1, 2, 3], {0: 0, 1: 1, 2: 1, 3: 0})
    check("exact_coincident_and_near_neighbor", p_nn == [(0, 1), (2, 3)] and amb_nn == [])

    # 2. Equal distance ambiguity (gap < GAP_EPS fails closed into ambiguous)
    coords_eq = [(0.0, 1.0, 1.0), (0.0, 1.00001, 1.0), (0.0, 0.99999, 1.0)]
    p_eq, _, amb_eq = seam_pairs(coords_eq, [0, 1, 2], {0: 0, 1: 1, 2: 1})
    check("equaldist_ambiguity", 0 in amb_eq and p_eq == [])

    # 3. Mutual mismatch (vertex nearest neighbor does not mutually reciprocate)
    coords_mm = [(0.0, 1.0, 1.0), (0.0, 1.00002, 1.0), (0.0, 1.00003, 1.0)]
    p_mm, _, amb_mm = seam_pairs(coords_mm, [0, 1, 2], {0: 0, 1: 1, 2: 0})
    check("mutual_mismatch", p_mm == [(1, 2)] and amb_mm == [0])

    # Prior cases
    coords = [(0.00001, 1., 1.), (-0.00001, 1., 1.), (1., 2., 3.), (0., 4., 4.)]
    pairs, _, _ = seam_pairs(coords, [0, 1, 3], {0: 0, 1: 1, 3: 2})
    check("one_cross_component_pair", pairs == [(0, 1)])
    check("repeat_is_deterministic", seam_pairs(coords, [3, 1, 0], {0: 0, 1: 1, 3: 2})[0] == pairs)
    try:
        _, _, amb = seam_pairs([(0., 0., 0.), (0., 0., 0.), (0., 0., 0.)], [0, 1, 2], {0: 0, 1: 1, 2: 2})
        check("ambiguous_case_detected", len(amb) == 3)
    except ValueError:
        fail("ambiguous case raised unexpectedly")
    check("no_match_is_empty", seam_pairs([(0., 0., 0.), (0., 1., 0.)], [0, 1], {0: 0, 1: 1})[0] == [])
    check("same_component_does_not_pair", seam_pairs(coords, [0, 1], {0: 7, 1: 7})[0] == [])
    try:
        seam_pairs([(float("nan"), 0., 0.)], [0], {0: 0})
        fail("non-finite coordinate did not fail")
    except ValueError as exc:
        check("nonfinite_fails", "non-finite" in str(exc))

    return {"passed": True, "checks": tests}


def emit(receipt, code):
    sys.stdout.write(json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n")
    return code


def parse_args():
    raw = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("audit", "seam-preparation"), default="audit")
    parser.add_argument("--input")
    parser.add_argument("--output")
    parser.add_argument("--receipt")
    parser.add_argument("--selftest", action="store_true")
    return parser.parse_args(raw)


def main():
    args = parse_args()
    receipt = {"success": False, "status": "error", "mode": args.mode, "source": None,
               "derived": None, "body": None, "other_meshes": {}, "errors": []}
    if args.selftest:
        try:
            receipt.update(status="selftest_passed", success=True, selftest=run_selftest())
            return emit(receipt, EXIT_OK)
        except Exception as exc:
            receipt["errors"].append(str(exc))
            return emit(receipt, EXIT_FAILED)
    if not args.input:
        receipt["status"] = "invalid_arguments"
        receipt["errors"].append("--input is required")
        return emit(receipt, EXIT_USAGE)
    source = resolved(args.input)
    if not os.path.isfile(source):
        receipt["status"] = "input_missing"
        receipt["errors"].append("Input file does not exist: " + source)
        return emit(receipt, EXIT_USAGE)
    # Validate all write destinations before opening Blender or reaching any
    # receipt-writing success/failure path. samefile also catches hardlinks.
    path_error = receipt_path_error(source, args.output, args.receipt)
    if path_error:
        receipt["status"] = "receipt_path_refused"
        receipt["errors"].append(path_error)
        return emit(receipt, EXIT_USAGE)
    if args.mode == "seam-preparation":
        if not args.output:
            receipt["status"] = "invalid_arguments"
            receipt["errors"].append("--output is required in seam-preparation mode")
            return emit(receipt, EXIT_USAGE)
        output = resolved(args.output)
        if paths_alias(output, source):
            receipt["status"] = "same_path_refused"
            receipt["errors"].append("Output resolves to the source path; source is read-only")
            return emit(receipt, EXIT_USAGE)
        if os.path.exists(output):
            receipt["status"] = "output_exists_refused"
            receipt["errors"].append("Refusing to overwrite existing derivative: " + output)
            return emit(receipt, EXIT_USAGE)
    elif args.output and paths_alias(args.output, source):
        receipt["status"] = "same_path_refused"
        receipt["errors"].append("Output resolves to the source path; source is read-only")
        return emit(receipt, EXIT_USAGE)
    if bpy is None:
        receipt["status"] = "missing_bpy"
        receipt["errors"].append("Run this script with Blender's Python: bpy is unavailable.")
        return emit(receipt, EXIT_MISSING_BPY)
    source_hash = digest(source)
    receipt["source"] = {"path": source, "sha256": source_hash}
    try:
        bpy.ops.wm.open_mainfile(filepath=source, load_ui=False)
        body = bpy.data.objects.get("Smalltrotter_Body")
        if body is None or body.type != "MESH":
            fail("expected mesh object Smalltrotter_Body")
        if body.library is not None or body.data.library is not None:
            fail("body mesh is linked data; refusing to edit")
        multi = [m for m in body.modifiers if m.type == "MULTIRES"]
        bmdata = collect_body(body)
        receipt["body"] = {"object": body.name, "multires_levels": [
            {"name": m.name, "total_levels": m.total_levels, "levels": m.total_levels,
             "sculpt_levels": m.sculpt_levels, "render_levels": m.render_levels} for m in multi],
            "source": {k: v for k, v in bmdata.items() if k != "component_ids"}}
        for obj in sorted((o for o in bpy.context.scene.objects if o.type == "MESH" and o != body), key=lambda o: o.name):
            receipt["other_meshes"][obj.name] = {"topology": topology(obj.data), **mesh_fingerprints(obj.data)}
        if args.mode == "audit":
            receipt.update(status="audit_complete", success=True)
            if args.receipt:
                with open(args.receipt, "w", encoding="utf-8") as f:
                    json.dump(receipt, f, indent=2, sort_keys=True)
            return emit(receipt, EXIT_OK)
        pairs = bmdata["candidate_pairs"]
        if bmdata["ambiguous_candidate_vertex_count"]:
            fail("ambiguous seam candidates at vertex indices " + ",".join(map(str, bmdata["ambiguous_candidate_indices_sample"][:16])))
        if not pairs:
            fail("no unambiguous cross-component seam pairs found; no derivative created")
        if len(pairs) != 824:
            fail("expected exactly 824 seam pairs before weld, found %d" % len(pairs))
        if len(bmdata["component_ids"]) == 0 or len(set(bmdata["component_ids"].values())) != 2:
            fail("expected exactly two body components before seam preparation")
        # Validate target groups before any destructive operation.
        mesh = body.data
        bmesh_obj = bmesh.new()
        bmesh_obj.from_mesh(mesh)
        bmesh_obj.verts.ensure_lookup_table()
        for a, b in pairs:
            va, vb = bmesh_obj.verts[a], bmesh_obj.verts[b]
            if bmdata["component_ids"][a] == bmdata["component_ids"][b]:
                fail("paired vertices belong to the same connected component")
            if va.co.x * vb.co.x > 0 or abs(va.co.x) > PLANE_EPS or abs(vb.co.x) > PLANE_EPS:
                fail("pair fails symmetry-plane/opposite-side validation")
        # Remove the Multires modifier only from this derivative before any
        # topology edit. Preserve the mesh datablock as a separate copy.
        mesh_copy = mesh.copy()
        body.data = mesh_copy
        for modifier in list(body.modifiers):
            if modifier.type == "MULTIRES":
                body.modifiers.remove(modifier)
        # Recreate map against copied mesh and use explicit directed targetmap.
        bmesh_obj.free()
        work = bmesh.new(); work.from_mesh(body.data); work.verts.ensure_lookup_table()
        targetmap = {}
        for a, b in pairs:
            va, vb = work.verts[a], work.verts[b]
            # The positive-X vertex is the deterministic target; negative-X
            # vertex is mapped into it. No broad distance-based welding.
            if va.co.x > 0 or (va.co.x == 0 and a < b):
                target, source_v = va, vb
            else:
                target, source_v = vb, va
            targetmap[source_v] = target
        bmesh.ops.weld_verts(work, targetmap=targetmap)
        work.to_mesh(body.data); work.free(); body.data.update()
        after = topology(body.data)
        if after["vertices"] != bmdata["topology"]["vertices"] - len(pairs):
            fail("weld did not remove exactly one vertex per seam pair")
        if after["boundary_edges"] != 0 or after["nonmanifold_edges"] != 0:
            fail("derivative body must have zero boundary and over-connected edges")
        if after["components"] != 1:
            fail("expected 1 connected component after weld, got %d" % after["components"])
        if after["faces"] != bmdata["topology"]["faces"]:
            fail("face count changed after weld: before %d, after %d" % (bmdata["topology"]["faces"], after["faces"]))
        source_bounds = bmdata["topology"]["bounds"]
        for i in range(3):
            if abs(after["bounds"]["min"][i] - source_bounds["min"][i]) > PLANE_EPS or \
               abs(after["bounds"]["max"][i] - source_bounds["max"][i]) > PLANE_EPS:
                fail("bounds changed after weld beyond tolerance")
        receipt["body"]["derivative"] = {"topology": after, "welded_pairs": len(pairs),
            "multires_removed_from_derivative": len(multi), "method": "explicit bmesh.ops.weld_verts targetmap"}
        os.makedirs(os.path.dirname(output), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=output)
        expected_other = {name: (item["coordinate_index_sha256"], item["polygon_index_sha256"])
                          for name, item in receipt["other_meshes"].items()}
        bpy.ops.wm.open_mainfile(filepath=output, load_ui=False)
        saved_body = bpy.data.objects.get("Smalltrotter_Body")
        if saved_body is None or saved_body.type != "MESH":
            fail("saved derivative is missing body mesh")
        if topology(saved_body.data) != after:
            fail("saved derivative body topology differs from verified result")
        if any(m.type == "MULTIRES" for m in saved_body.modifiers):
            fail("saved derivative unexpectedly retains Multires")
        receipt["body"]["derivative"]["reopened_topology_verified"] = True
        for name, expected in expected_other.items():
            saved_obj = bpy.data.objects.get(name)
            if saved_obj is None or saved_obj.type != "MESH":
                fail("saved derivative is missing unchanged mesh " + name)
            actual = mesh_fingerprints(saved_obj.data)
            if (actual["coordinate_index_sha256"], actual["polygon_index_sha256"]) != expected:
                fail("saved derivative changed vertex coordinates or polygon indices for " + name)
        # Hash verification after save: source file remains unchanged.
        end_source_hash = digest(source)
        if end_source_hash != source_hash:
            fail("source hash changed during processing")
        receipt["derived"] = {"path": output, "sha256": digest(output)}
        receipt["success"] = True; receipt["status"] = "derivative_created"
        if args.receipt:
            with open(args.receipt, "w", encoding="utf-8") as f:
                json.dump(receipt, f, indent=2, sort_keys=True)
        return emit(receipt, EXIT_OK)
    except Exception as exc:
        receipt["status"] = "failed_closed"
        receipt["errors"].append(str(exc))
        print(traceback.format_exc(), file=sys.stderr)
        if args.receipt:
            try:
                with open(args.receipt, "w", encoding="utf-8") as f:
                    json.dump(receipt, f, indent=2, sort_keys=True)
            except Exception as write_exc:
                print("receipt write failed: " + str(write_exc), file=sys.stderr)
        return emit(receipt, EXIT_FAILED)


if __name__ == "__main__":
    sys.exit(main())
