# Smalltrotter source preparation utility

`scripts/assets/prepare_smalltrotter_source.py` provides read-only audit mode and a conservative derivative-only seam-preparation mode. Run it with Blender and `--disable-autoexec`, for example:

```powershell
blender.exe -b --disable-autoexec --python stalthos-runtime/scripts/assets/prepare_smalltrotter_source.py -- --mode audit --input var/smalltrotter-exact-seam/master-copy.blend --receipt var/smalltrotter-exact-seam/audit.json
```

## Methodology & Matching Algorithm

Audit opens the given file without saving it, requires the named `Smalltrotter_Body` mesh, and reports base topology, bounds, boundary counts, connected-component sizes, Multires levels, seam candidates, and coordinate/polygon fingerprints for the other mesh objects (`Smalltrotter_Eyes`, `Smalltrotter_Bristles`).

### Seam Candidate Pairing: Mutual-Unique-Nearest Matching

The bilateral seam pairing algorithm operates strictly on `Smalltrotter_Body` boundary vertices:
1. **Plane Proximity Filter**: Only boundary vertices within `PLANE_EPS = 1.0e-4` m of the local $X=0$ plane are considered.
2. **Opposite Side & Component Guard**: A candidate pair $(a, b)$ is valid only if $a$ and $b$ belong to different connected components (`component_ids[a] != component_ids[b]`) and lie on opposite sides or at zero ($X_a \cdot X_b \le 0.0$).
3. **Search Radius**: Opposing candidates within Euclidean distance `PAIR_EPS = 1.0e-4` m are gathered.
4. **Strict Gap Disambiguation**:
   - In regions where valid sculpt topology has edges shorter than `PAIR_EPS` (such as the 4 clusters on the tail with edge lengths $\sim 1.53 \times 10^{-5}$ m to $8.09 \times 10^{-5}$ m), a vertex may see both its exact counterpart and an adjacent boundary vertex within `PAIR_EPS`.
   - When a vertex has multiple candidate counterparts, candidates are sorted by distance ascending.
   - The nearest candidate $b_1$ at distance $d_1$ is accepted as the unique nearest neighbor only if the gap to the second-nearest candidate $b_2$ satisfies $d_2 - d_1 \ge \text{GAP\_EPS}$ where `GAP_EPS = 1.0e-6` m (1 micron).
   - If $d_2 - d_1 < \text{GAP\_EPS}$, the vertex fails closed as ambiguous (genuine equal or near-equal distance ambiguity).
5. **Mutual Reciprocity Guard**:
   - Pairing must be mutually unique: $a$'s unique nearest neighbor must be $b$, AND $b$'s unique nearest neighbor must be $a$.
   - If $a$ selects $b$, but $b$ selects another vertex $c$ ($c \ne a$), or if $b$ is ambiguous, a mutual mismatch occurs. The unmatched vertex is flagged as ambiguous and fails closed.
6. **Disjoint Assignment**: Every accepted pair is disjoint; no vertex is paired more than once.

### Derivative Invariants

Seam-preparation requires a separate output path that does not already exist and does not resolve to the source path.
- Invariants checked before weld:
  - Exactly 824 pairs must be identified.
  - Zero ambiguous vertices (`ambiguous_candidate_vertex_count == 0`).
  - Exactly two source components.
  - Symmetry plane and opposite-side coordinates validated for all 824 pairs.
- Derivative construction:
  - The base mesh is copied; Multires modifier is removed ONLY from the derived base before welding.
  - Welding is performed via explicit `bmesh.ops.weld_verts` using a directed target map (mapping negative-X into positive-X, or lower index if $X=0$).
  - No broad distance-based vertex merging, remeshing, material, or rig modifications are performed.
- Post-weld inspection:
  - Connected components must equal exactly 1.
  - Face polygon count must be preserved exactly (33,318 faces).
  - Bounding box must match source bounds within float tolerance.
  - Saved derivative is reopened from disk in a fresh Blender session to verify that `Smalltrotter_Eyes` and `Smalltrotter_Bristles` coordinate and polygon hashes match the source master exactly.
  - Source master file hash is verified unchanged.

## Test Verification (`--selftest`)

`--selftest` uses explicit exception checks (`if not condition: raise RuntimeError(...)`) rather than `assert`, guaranteeing that tests remain fully active when executed with optimized Python (`python -O`).

Tested cases (9 active checks):
1. `exact_coincident_and_near_neighbor`: Verifies that an exact coincident counterpart ($d=0$) is uniquely selected over a topological neighbor on the opposing component ($d=2.0\times 10^{-5}\text{ m} \ge \text{GAP\_EPS}$), resolving pairs without ambiguity.
2. `equaldist_ambiguity`: Verifies that genuine equal-distance candidates ($|d_2 - d_1| < \text{GAP\_EPS}$) fail closed into `ambiguous` with no pairs formed.
3. `mutual_mismatch`: Verifies that when vertex $A$ prefers $B$, but $B$ prefers $C$, $A$ is flagged as ambiguous and only genuine mutual pairs are retained.
4. `one_cross_component_pair`: Verifies single cross-component pairing.
5. `repeat_is_deterministic`: Verifies index order permutation does not alter candidate pairing.
6. `ambiguous_case_detected`: Verifies three coincident vertices at $(0,0,0)$ trigger ambiguity detection.
7. `no_match_is_empty`: Verifies distant vertices ($d > \text{PAIR\_EPS}$) produce empty pairs and empty ambiguous list.
8. `same_component_does_not_pair`: Verifies vertices within the same component never pair.
9. `nonfinite_fails`: Verifies non-finite coordinates (NaN/inf) fail with `ValueError`.

## Historical Blocked Evidence (Audit Failure Record)

Prior to the introduction of `GAP_EPS` mutual-unique-nearest matching, the naive `degree > 1` rule flagged 16 boundary vertices as ambiguous in the mid-to-distal tail:
- Sample ambiguous indices: `2610, 2611, 2737, 2745, 2747, 2835, 3554, 3555, 3681, 3689, 3691, 3779, 3895, 3897, 4378, 4380`.
- The audit found 816 candidate pairs and 16 ambiguous vertices, halting seam preparation with exit code 5 (`failed_closed`) without producing a derivative.
- Diagnosis in `agy-smalltrotter-seam-diagnosis` proved that each of these 16 vertices had an exact coincident counterpart ($d = 0.00000000$ m) and a secondary topological neighbor at $d \ge 1.53 \times 10^{-5}$ m. The ambiguity was purely an artifact of sub-millimeter edge lengths along the tail falling within the `1.0e-4` m pairing radius.

## Actual Execution Receipts on Master Copy

Blender 5.2.0 LTS was executed against an exact copy of the tracked rough master:
- **Tracked Master**: `assets/source/creatures/smalltrotter/smalltrotter-rough-master.blend`
- **Tracked Master SHA-256 (pre- and post-run)**: `B987E7D7EAC4DFE0E09E2E188A592236ED57EB5E00DA7D8F7E6D1D901DE15350`
- **Master Copy**: `var/smalltrotter-exact-seam/master-copy.blend` (SHA-256 identical: `B987E7D7EAC4DFE0E09E2E188A592236ED57EB5E00DA7D8F7E6D1D901DE15350`)
- **Derived Output**: `var/smalltrotter-exact-seam/smalltrotter-prepared-derivative.blend`
- **Derived Output SHA-256**: `021a0e773fa13edfce4b0f32c00ef13b34281e7a94bb2b6ae57c3e92c774627b`

### Audit Receipt (`var/smalltrotter-exact-seam/audit.json`)
- `status`: `audit_complete`, `success`: `true`
- `boundary_vertex_count`: 1,648
- `near_plane_boundary_candidate_count`: 1,648
- `candidate_pair_count`: 824
- `ambiguous_candidate_vertex_count`: 0
- All 16 previously ambiguous vertices paired exactly:
  - `(2610, 3554)` ($d = 0.00000000$ m)
  - `(2611, 3555)` ($d = 0.00000000$ m)
  - `(2737, 3681)` ($d = 0.00000000$ m)
  - `(2745, 3689)` ($d = 0.00000000$ m)
  - `(2747, 3691)` ($d = 0.00000000$ m)
  - `(2835, 3779)` ($d = 0.00000000$ m)
  - `(3895, 4378)` ($d = 0.00000000$ m)
  - `(3897, 4380)` ($d = 0.00000000$ m)

### Seam Preparation Receipt (`var/smalltrotter-exact-seam/seam-derivative-receipt.json`)
- `status`: `derivative_created`, `success`: `true`
- `welded_pairs`: 824
- `multires_removed_from_derivative`: 1
- **Source Body Base Cage**:
  - Vertices: 34,144
  - Edges: 67,460
  - Faces: 33,318
  - Components: 2 (17,072 vertices each)
  - Boundary edges: 1,648
  - Nonmanifold edges: 0
  - Bounds: min `[-0.487511, -1.711490, 0.0]`, max `[0.487511, 3.047774, 2.598147]`
- **Derived Body Base Cage**:
  - Vertices: 33,320 ($34,144 - 824$)
  - Edges: 66,636 ($67,460 - 824$)
  - Faces: 33,318 (100% face polygon count preserved)
  - Components: 1 (single continuous manifold mesh)
  - Boundary edges: 0 (completely sealed along bilateral seam)
  - Nonmanifold edges: 0
  - Bounds: min `[-0.487511, -1.711490, 0.0]`, max `[0.487511, 3.047774, 2.598147]` (identical)
- **Untouched Other Meshes (Saved & Reopened Invariant Verification)**:
  - `Smalltrotter_Eyes`:
    - Vertices: 964, Edges: 1,984, Faces: 1,024, Components: 2, Boundary edges: 0
    - Coordinate SHA-256: `9d2eb00c705413cfadc154814b1318796f8c76cc8d585ce2a4d14f1652efc5c6` (exact match)
    - Polygon SHA-256: `80c356ebcffdffe5df4e70300d847800ca261f247534ba7b7d6fdd5138e9b3c2` (exact match)
  - `Smalltrotter_Bristles`:
    - Vertices: 40,300, Edges: 78,000, Faces: 39,000, Components: 1,300, Boundary edges: 6,500
    - Coordinate SHA-256: `ac7bb4cb728d9ec4ee1116027ec56e30d5b31b119b78227d191321d3f2d73a41` (exact match)
    - Polygon SHA-256: `981e2677ff706ffbf998b873a4d10b6c23228e25a1a0d7d3c94f8238bb39a748` (exact match)
- **Source Byte Invariant**:
  - `smalltrotter-rough-master.blend` SHA-256 is unchanged: `B987E7D7EAC4DFE0E09E2E188A592236ED57EB5E00DA7D8F7E6D1D901DE15350`.

## Scope & Non-Acceptance Notice

This source preparation derivative establishes a clean, single-component welded base cage with zero bilateral seam boundary edges for downstream asset pipelines. In accordance with project conventions:
- No claim is made regarding final art direction, proportion sign-off, texturing, or anatomical plausibility.
- No claim is made regarding skinning weights, deformation behavior under animation, or rig compatibility.
- No claim is made regarding 30 FPS rendering performance or engine acceptance.

