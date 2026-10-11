# Independent review: Luna Wicked build and LOD proposals

Scope: read-only review of `var/worktrees/luna-wicked-build/stalthos-runtime/scripts/{Build-Wicked.ps1,Prepare-Wicked.ps1,Build.ps1}`, its focused PowerShell test, and `var/worktrees/luna-lod-fix/stalthos-toolkit/src/stalthos_toolkit/optimize/{lods.py,decimate.py}`. Pinned source checked at `var/vendor/WickedEngine`, HEAD `df44c3db4c4927492bc9c791eac715d98d7ed091`, clean working tree. No engine install, GPU run, source edit, or full suite.

## Findings

1. **Blocker: fresh engine build requests a non-buildable CMake alias.** `Build-Wicked.ps1:69` invokes `cmake --build ... --target WickedEngine`. In the pinned source, `WickedEngine/WickedEngine/CMakeLists.txt:131-136` sets `TARGET_NAME` to `WickedEngine_emb_shaders` when embedded shaders are enabled and declares `WickedEngine` as an `ALIAS`. CMake aliases are not generated build targets. This script enables embedded shaders at line 46, so request the concrete target (`WickedEngine_emb_shaders`) or build the default aggregate. The mock test accepts every argument and therefore misses this failure.

2. **Blocker: the cache import looks for dxcompiler at a different path from the fresh build exporter.** `Build-Wicked.ps1` stages `bin/dxcompiler.dll` from `$build/WickedEngine/dxcompiler.dll` (mapping near lines 73-79). `Prepare-Wicked.ps1:54` instead imports `Release/dxcompiler.dll` from the shared build. Pinned CMake copies the DLL into the `WickedEngine` subdirectory's current binary directory (`WickedEngine/WickedEngine/CMakeLists.txt:293`), not its `Release` configuration subdirectory. A freshly built cache passed to `Prepare-Wicked.ps1 -ReleaseCache` will fail to find the DLL. Align the input path and add a fixture that uses the real build-tree layout.

3. **Blocker: Prepare-Wicked mutates a prior cache in place before it knows publication can finish.** In `Prepare-Wicked.ps1:55-70`, verified files are copied one by one directly into `$cacheOut`, then the cache file and receipt are overwritten. If a later input is missing, unreadable, or copy/hash fails, already copied files have replaced prior data while the prior receipt may still be present. Stage and validate the complete new cache, then publish with backup/rollback (as Build-Wicked does) so failures preserve the previous cache and receipt.

4. **Freshness claim is not enforced.** `Build-Wicked.ps1` configures and builds in a caller-reusable `$build` directory and does not clean it or reject an existing tree, yet labels the receipt `build_kind='fresh'`. An incremental build can reuse stale objects and still pass the exact-80-object check. Ensure the compile starts from a clean build tree (or use `--clean-first` and prove all expected objects were regenerated) before issuing a fresh-build receipt.

5. **LOD global hard cap is not global across primitives.** `lods.py:431-434` calls `_trim_document` when total triangles exceed `target_triangles`; `_trim_document` at lines 309-322 applies `target_triangles` independently to every primitive. It can therefore leave the document above the intended total. The supplied structural trial reports 2,592 source triangles and LOD1 target 40% (=1,036), but output 1,536 triangles (ratio 0.59259). Because its two primitives are each below the 1,036 per-primitive cap, the supposed corrective trim is a no-op. Enforce a shared remaining budget across primitives and add a multi-primitive test asserting total output <= requested total.

## Checks performed

- Confirmed the vendored Wicked source is exactly the requested pinned revision and has no local modifications.
- Read its target declaration and DLL copy rule directly.
- Ran the focused test `pwsh -NoLogo -NoProfile -File var/worktrees/luna-wicked-build/stalthos-runtime/tests/Build-Wicked.Tests.ps1`; it passed its fake compile/publication scenario. It does not invoke real CMake and its fake build does not validate `--target`.
- Reviewed the provided `var/reports/gltfpack-readiness/result.json` LOD counts against the proposed target and trim logic. No visual-quality inference is made.
- Did not run a real engine configure/build or the full suite; the root fullsuite baseline was already running.
