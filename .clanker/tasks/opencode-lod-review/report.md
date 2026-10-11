# LOD Review — Root Cause Report

**Scope:** read-only inspection of `optimize/lods.py`, `commands/generate_lods.py`, `optimize/decimate.py`, `optimize/tools.py`. I did **not** run code or edit files; the Softbough counts are the ones given in the brief, and the mechanism below is derived from source, not observed at runtime. Unverified points are marked.

## Verdict

The zero-triangle LOD2 is not a gltfpack problem. It is a **degenerate vertex-clustering bug in the pure-python fallback** (`decimate.py`), triggered by the per-primitive decimation and the monotonic-shrink guard in `lods.py`. The chain fails because it validates *strictly decreasing, non-empty* LODs (`lods.py:162-164`, `lods.py:530-549`), and the last level comes out empty.

## Precise cause

**1. `_divisions_for` can return `divisions=1`, which collapses the whole mesh to a single vertex.**
`decimate.py:75-89` picks the grid whose `estimate = divisions**3 * bounds_cells` is closest to the target, scanning `range(1, 33)` (line 84). Callers always pass `bounds_cells=1.0` (`decimate.py:182`). For target vertices ≤ ~4, `1**3 = 1` is closer to the target than `2**3 = 8`, so it returns **1**. With `divisions=1`, the cell key at `decimate.py:67` is `min(0, …) = (0,0,0)` for every vertex — one cell. `_cluster_mesh` (lines 195-216) maps every vertex to the same index, so every triangle hits the `a == b or b == c or a == c` guard (lines 214-215) and is skipped: **output = 0 triangles**.

**2. `target_vertices` floors at 4, which is exactly inside the degenerate zone.**
`cluster_decimate` (lines 173-174) computes `target_triangles = max(1, floor(src*ratio))` then `target_vertices = max(4, ceil(target_triangles/2))`. The loop calls `_divisions_for(max(4, target_vertices // 2), 1.0)` (line 182). Any primitive whose target is ≤ ~8 triangles yields arg = 4 → divisions = 1 → primitive is wiped.

**3. `lods.py` decimates per primitive from the *source*, compounding the wipe.**
`_decimate_document` (`lods.py:292-306`) loops each mesh/primitive and calls `cluster_decimate(arrays, ratio)` with the **whole-document** ratio. Small primitives get `floor(p_tris * ratio) → max(1, 0) = 1` target, i.e. the worst case above. A plant is many small leaf primitives, so most are zeroed at LOD1.

**4. The monotonic guard then starves later levels.**
`lods.py:416-423`: `target_triangles = min(int(base*ratio), previous_triangles - 1)`. Because LOD1 over-collapsed (explaining the observed `2592 / 23` at two levels — a target ratio of 0.40 that *achieved* ~0.009), LOD2's budget becomes `min(388, 23-1) = 22`, i.e. ratio `22/2592 = 0.0085`. At that ratio every surviving primitive again falls under the target ≤8 floor → divisions=1 → **LOD2 = 0** (and LOD3 then dies either here or at `lods.py:419-423` "triangle budget exhausted").

**5. Nothing rescues undershoot.** `cluster_decimate` only calls `_drop_smallest_triangles` when still *over* budget (`decimate.py:184-188`), and that function returns immediately for 0 triangles (`decimate.py:124`). There is no lower-bound guard, so an empty result is passed through and only caught later as `non_monotonic_lods` (`lods.py:533-535`) → `ValidationFailed` (`generate_lods.py:218-222`, exit 3). The two-level chain "succeeds" only because its final level (`23`) is non-empty and strictly below `2592`.

## Production correction (proposed)

Fix the fallback so it *never* emits an empty or catastrophically under-budget level; the chain then holds monotonicity by construction.

1. **Never allow a full collapse.** In `cluster_decimate` (`decimate.py:165`), after clustering/dropping, if `working.triangle_count == 0` or `< max(1, target_triangles // 2)`, fall back to area-based trimming: `_drop_smallest_triangles(mesh, target_triangles)` on the original mesh. That path deterministically keeps the largest-area triangles and returns exactly `min(target, source) > 0` for any non-empty input.
2. **Kill the `divisions=1` degenerate branch.** In `_divisions_for` (`decimate.py:75`), floor the result at `2` (or skip `divisions=1`), and derive the occupancy from a real first pass rather than the constant `bounds_cells=1.0`; clamp so the chosen grid's occupied-cell count is ≤ `target_vertices`.
3. **Make `lods.py` chain-robust.** Either (a) decimate each LOD from the *previous* LOD with a relative ratio so budgets never collapse to ~0.0085, or (b) if a level would be forced below a floor ≥ 1 triangle, stop the chain early with a warning (`result.warnings`) instead of emitting empty/duplicate levels. Additionally require every record `triangles >= 1` and `achieved_ratio` within a tolerance of the requested ratio; the fallback is already degraded, so flag it explicitly rather than failing only at validation.
4. **Consider iterative-source fix specifically** for the observed 0.40→0.009 overshoot: the per-primitive grid is simply too coarse; the occupancy-based `_divisions_for` (fix 2) is the direct remedy.

## Tests to add (no Blender, no network)

- Unit `cluster_decimate` on synthetic meshes for ratios `[1.0, 0.4, 0.15, 0.05]`: assert `triangle_count >= 1` and `achieved_ratio` within e.g. `[0.5*requested, 2*requested]`. Regression: a primitive small enough to hit the floor must not return 0.
- Unit `_divisions_for`: assert it never returns 1 for any target, or assert that clustering a 4+ vertex mesh never yields 0 triangles.
- Multi-primitive `build_lod_chain(..., prefer="python", allow_degraded=True)` with one large + several tiny primitives: assert all records `>= 1` and `chain.monotonic()` is `True`.
- Fixture reproducing `2592` triangles: assert chain is strictly decreasing and non-empty and no `ValidationFailed`.
- Status-envelope test through `generate_lods`: exit code and `status.metrics["lod_count"] == 4`, `monotonic == True`.

## gltfpack (free/open-source optimizer) — install & discovery

- **What:** `gltfpack` ships with meshoptimizer (**MIT**, `https://github.com/zeux/meshoptimizer#gltfpack`). Prebuilt Windows `gltfpack.exe` is on the project's GitHub Releases; no paid service. (Optionally `npm i -g gltfpack`.)
- **Discovery today:** `TOOL_CANDIDATES["gltfpack"] = ("gltfpack", "gltfpack.exe")` (`tools.py:27`), resolved via `shutil.which` in `locate` (`tools.py:83-92`), plus an explicit override env var `STEALTHOS_GLTFPACK` (`ENV_PREFIX` `tools.py:23`, `_env_override` `tools.py:73-80`).
- **Fix for the user:** download the meshoptimizer release, put `gltfpack.exe` on `PATH` (or set `STEALTHOS_GLTFPACK=C:\path\gltfpack.exe`), and re-run. Existing hint: `tools.py:38`. Optionally vendor the binary under a repo `tools/` dir. When present, `use_gltfpack` (`lods.py:386-392`) routes LOD1+ through `run_gltfpack_simplify` and the python fallback is skipped entirely, so the bug above becomes moot — but the fallback still needs the fix because it is the documented degraded path.

## Assumptions / still to validate

- **Unverified at runtime:** exact Softbough primitive count/sizes; whether the observed failure is the LOD2 validation or the LOD3 `triangle budget exhausted` error. Both trace to the same mechanism, but you should capture the actual `structural_issues`/stack.
- **`glb.py` not read:** `MeshArrays.triangle_count`, `GlbDocument.summary()/primitive_arrays`, and `run_gltfpack_simplify` (argv/version) are assumed. The `-si <ratio>` integration point should be confirmed.
- The `bounds_cells` fill factor is always `1.0` at call sites (`decimate.py:182`); I could not confirm whether a caller ever passes anything else.
- `_divisions_for`'s occupancy estimate assumes ~`divisions**3` vertices; for planar/UV-seam-heavy plant meshes the real occupancy is much lower, so the grid is systematically too coarse — this over-collapse is why an LOD1 target of 0.40 lands near 0.009, but the exact achieved ratio for Softbough needs a real run to confirm.