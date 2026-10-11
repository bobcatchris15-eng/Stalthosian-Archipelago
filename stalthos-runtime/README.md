# The Stalthosian Archipelago — runtime construction

**Decided:** Wicked Engine, native C++17, Windows DX12. **Target:** a full draft with all five playable lives and all four islands. The current executable is an initial diagnostic host, not that completed draft.

## Run this checkout

From the repository root in PowerShell:

```powershell
./stalthos-runtime/scripts/Build.ps1
./var/build/stalthos-runtime/Release/StalthosScenarios.exe
./stalthos-runtime/scripts/Run.ps1 -Duration 12
./stalthos-runtime/scripts/Run.ps1 -Interactive
```

Click the window for input. W/S move along the animal's heading; A/D turn its body. F1/F2/F3 change the camera while preserving body-relative steering. Close the window to end interactive runs. An optional `-Plant <absolute-glb-path>` imports a toolkit-produced plant. Duration must be positive and at most 120 seconds for bounded diagnostics.

`config/wicked.lock.json` pins MIT-licensed Wicked v0.72.120, revision `df44c3db4c4927492bc9c791eac715d98d7ed091`. Source is cloned into ignored `var/vendor/WickedEngine`; the original project supplying a local mirror remains untouched. `Prepare-Wicked.ps1 -ReleaseCache <path>` can import a compatible existing Release build into the project-local cache. The import stages all 80 common objects, four static libraries, and `dxcompiler.dll`, verifies their SHA-256 values, then publishes the cache as one recoverable replacement. A failed import leaves the previous cache and its receipt intact. This remains verified cache reuse, separate from a fresh engine compile.

To reproduce the preparation used on this machine:

```powershell
./stalthos-runtime/scripts/Prepare-Wicked.ps1 -SourceMirror 'E:/The Jaetz/rendering-research/vendor/WickedEngine' -ReleaseCache 'E:/The Jaetz/rendering-research/vendor/wicked-build'
```

Without the mirror, preparation clones official upstream at the pin. Without `-ReleaseCache`, it prepares source only. Compatible existing build trees place `dxcompiler.dll` at `WickedEngine/dxcompiler.dll`; older imported caches with `Release/dxcompiler.dll` are also supported. CMake is discovered on PATH or at the installed VS 2026 BuildTools location. Fresh compilation and host builds are presently Windows/VS 2026 specific.

To compile the pinned engine locally and publish a validated Release cache:

```powershell
./stalthos-runtime/scripts/Prepare-Wicked.ps1
./stalthos-runtime/scripts/Build-Wicked.ps1
./stalthos-runtime/scripts/Build.ps1
```

`Build-Wicked.ps1` configures the pinned source with embedded shaders, static libraries, AVX enabled and AVX2 disabled, builds the concrete `WickedEngine_emb_shaders` target in Release, and checks for the established 80 common object files plus four static libraries and the pinned `dxcompiler.dll`. It records SHA-256 values and compiler/generator provenance. A fresh compile refuses a populated build directory so stale objects cannot be represented as fresh work. Publication uses a staged cache directory and exclusive locks for the build and cache paths; an incomplete compile or failed validation leaves the previous cache in place. Competing runs that share either output path fail fast while its lock is held. `-Source`, `-BuildDirectory`, and `-CacheDirectory` let callers keep generated outputs under a worktree's ignored `var/` directory; `Build.ps1` accepts the same path overrides. The mocked PowerShell test checks the concrete target, real DLL layout, clean-build requirement, and preservation of a prior cache across import and compile failures. A clean compile on this checkout confirmed all 85 exported inputs; see var/build/wicked-fresh-release/build.log and var/vendor/wicked-fresh-release-cache/provenance.json.

Build outputs and logs live under `var/build/stalthos-runtime`. Each run creates a fresh directory under `var/reports/runtime`, with launch identity/executable hash, runtime receipt and a render-target screenshot after five seconds. The host copies the engine license beside its executable. Do not treat run-call counts as FPS; valid external frame-time capture is still required.

## Implemented foundation

`domain/foundation.hpp` separates responsive physical time from calendar scaling; provides body-relative steering, direct/destination/autonomous intent priority, separate body and camera stimuli, contact idempotence, deterministic region seeds, bounded retained identities, tree exceptions and hero-death continuity. Current heroes must use the explicit death transition; generic retirement/death cannot invalidate them. Six scenario groups cover these boundaries, including the independent review regressions.

The host uses the shared clock, direct authority, heading and initial hero identity. Other foundation cases are exercised by console scenarios and are not yet visible gameplay. State remains in memory: disk save/load, atomic recovery, lifecycle/ecology/streaming, real creature anatomy/animation, water, authored geography and game UI remain to implement. The blockout's analytical ground following is not collision/navigation acceptance.

See `../PROJECT-BUILD-PLAN.md` for the complete draft coverage and dependency order, and `../.clanker/current-work.md` for durable work state. Representative scene quality, moving images and target GTX 1080/Ryzen performance remain acceptance gates.


## Headless OpenCode dispatch

The user authorized `big-pickle` and `space-bunny` one-shot workers. Installed CLI: 1.18.35. Exact free-provider IDs: `opencode/big-pickle` and `opencode/space-bunny-free`. Specify `--agent build`: the configured default `orchestrator` is missing on this machine. Put the positional prompt before `--file`; that array option otherwise interprets the trailing prompt as another filename.

Task launchers under `.clanker/tasks/opencode-lod-review` and `.clanker/tasks/opencode-engine-review` use isolated checkouts under `var/worktrees`, `--pure`, JSON events and process results. Their process-local permissions allow read/glob/grep, deny edits and web tools, and put shell commands behind approval (`bash: ask`). Setting shell tools to deny reproduced a provider HTTP 403 free-tier error; switching to approval restored execution without approving any shell action. The brief explicitly requests read-only source analysis. Global OpenCode configuration was preserved. See upstream issue https://github.com/anomalyco/opencode/issues/53347 for matching permission-related reports; local events are the acceptance evidence.

These dispatch permissions describe a cooperative worker boundary, not an operating-system sandbox. Inspect actual final events and exit status before treating a worker as successful.

Verified 2026-10-11: a clean source compile under `var/build/wicked-fresh-release` published `var/vendor/wicked-fresh-release-cache`; the host rebuilt against it and rendered the baked Softbough GLB at 1920x1080. Evidence is under `var/reports/runtime/20261011-152740-b402c296`. `StalthosPersistence` adds tested bounded versioned DTO save files; host snapshot/save/load wiring and full region revision restoration are still pending. Run both native scenario executables through CTest.
