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

The host uses the shared clock, direct authority, heading and initial hero identity. Other foundation cases are exercised by console scenarios and are not yet visible gameplay. The diagnostic host now saves and restores foundation inventories, clocks and a ground-projected marker pose. Full animal state, recovery beyond checked file replacement, connected lifecycle/ecology/streaming, real creature anatomy/animation, finished water/scenery and game UI remain incomplete. The blockout's analytical ground following is not collision/navigation acceptance.

See `../PROJECT-BUILD-PLAN.md` for the complete draft coverage and dependency order, and `../.clanker/current-work.md` for durable work state. Representative scene quality, moving images and target GTX 1080/Ryzen performance remain acceptance gates.

## Headless OpenCode dispatch

The user authorized `big-pickle` and `space-bunny` one-shot workers. Installed CLI: 1.18.35. Exact free-provider IDs: `opencode/big-pickle` and `opencode/space-bunny-free`. Specify `--agent build`: the configured default `orchestrator` is missing on this machine. Put the positional prompt before `--file`; that array option otherwise interprets the trailing prompt as another filename.

Task launchers under `.clanker/tasks/opencode-lod-review` and `.clanker/tasks/opencode-engine-review` use isolated checkouts under `var/worktrees`, `--pure`, JSON events and process results. Their process-local permissions allow read/glob/grep, deny edits and web tools, and put shell commands behind approval (`bash: ask`). Setting shell tools to deny reproduced a provider HTTP 403 free-tier error; switching to approval restored execution without approving any shell action. The brief explicitly requests read-only source analysis. Global OpenCode configuration was preserved. See upstream issue https://github.com/anomalyco/opencode/issues/53347 for matching permission-related reports; local events are the acceptance evidence.

These dispatch permissions describe a cooperative worker boundary, not an operating-system sandbox. Inspect actual final events and exit status before treating a worker as successful.

Verified 2026-10-11: a clean source compile under `var/build/wicked-fresh-release` published `var/vendor/wicked-fresh-release-cache`; the host rebuilt against it and rendered the baked Softbough GLB at 1920x1080. Evidence is under `var/reports/runtime/20261011-152740-b402c296`. `StalthosPersistence` adds tested bounded versioned DTO save files; the subsequent checkpoint below adds host diagnostic save/load and exact region revision restoration. Run all four native scenario executables through CTest.

## Snapshot and landscape checkpoint — 2026-10-11

STL2 saves preserve independent physical/calendar clocks, authoritative identity inventories and exact region revisions. STL1 remains readable with documented fallbacks. The diagnostic host accepts these explicit local options:

```powershell

./stalthos-runtime/scripts/Run.ps1 -Duration 6 -DiagnosticWalk -SaveState var/saves/draft.sav

./stalthos-runtime/scripts/Run.ps1 -Duration 5 -LoadState var/saves/draft.sav -SaveState var/saves/draft.sav

```

The first actual two-process probe preserved the marker's x/z position and both saved clocks; calendar time continued from 5.831596699 to 10.669071901 seconds. Evidence: `var/reports/runtime-packet-three-continuity.json`. Save paths are restricted by the helper to this project's `var/`. The host requires a current live hero and recomputes the marker's vertical coordinate from the reference terrain; unsupported no-hero/vertical dynamics remain explicit limitations.

`domain/reference_landscape.hpp` supplies a provisional 192m Main Island patch with aligned creek, shoulders and bluff, deterministic habitat/placement queries, and wet-ground vegetation exclusion. The host samples its terrain, imports eight Softbough shrub instances and draws a bounded creek surface. Rock geometry is still provisional. This is a composed reference fixture, not complete island coverage or final water/scene acceptance.

The independently reviewed `domain/lifecycle.hpp` provides calendar-based hatchling/juvenile/adult/old/dead progression for five fictional profiles, terminal death and validated restore. Its pacing values are proposed draft defaults. The fourth packet connects this core to the native shared clock and STL3 optional hero lifecycle snapshot. Reproduction, parenting and needs remain unfinished. The neutral GLB retains zero animations; a separate deterministic walk-cycle GLB now exports one animation and 222 channels, with actual Wicked local pose playback evidence. Foot contact and low-tail clearance remain failed acceptance gates.

The walk-cycle toolkit changes are currently a local validation candidate. Native actor support accepts an external animated GLB; final toolkit suite completion is required before publishing the generator changes.

## Fifth packet — 2026-10-11

The previously pending full toolkit suite completed: 1954 passed, two skipped in 771.82 seconds. Tested candidate hashes matched; walk-generator changes were published in c93b264. Earlier pending-publication/test statements are historical. The authored walk remains a diagnostic draft with failed ground contact, not an accepted gait.

Jules needs 4917980815644905444, relationships 7316297015894091861 and activation 16396772999683175155 are completed remotely and now under isolated local review. Resume review-jules-needs, review-jules-relationships and review-jules-activation before new work. Scope/test receipts will determine integration. This wake started 18:16 America/Chicago, with a 35-minute cap and six local dispatch ceiling. Three local review dispatches used. The separate remote pool and lowest-capable-model policy remain authoritative.


## Reviewed Jules domain checkpoint

Final toolkit validation passed 1954 tests with two skips; walk-generator source is published in c93b264. The previous pending-toolkit statements are superseded. Three isolated reviewed Jules cores (needs, relationships, region activation) now pass root Release CTest 7/7. Resource contacts, parenting, persistence of these cores and actual Wicked region streaming remain unfinished. Gait grounding and explicit post-death successor loading are isolated current tasks. See current-work and packet-five receipt before continuation.
