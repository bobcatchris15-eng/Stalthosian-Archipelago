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

`config/wicked.lock.json` pins MIT-licensed Wicked v0.72.120, revision `df44c3db4c4927492bc9c791eac715d98d7ed091`. Source is cloned into ignored `var/vendor/WickedEngine`; the original project supplying a local mirror remains untouched. The current host builds against a project-local copy of an existing Release engine build: 80 common objects, four static libraries and dxcompiler.dll. Preparation and every build verify these 85 inputs by SHA-256. This is verified cache reuse, not a fresh engine rebuild.

To reproduce the preparation used on this machine:

```powershell
./stalthos-runtime/scripts/Prepare-Wicked.ps1 -SourceMirror 'E:/The Jaetz/rendering-research/vendor/WickedEngine' -ReleaseCache 'E:/The Jaetz/rendering-research/vendor/wicked-build'
```

Without the mirror, preparation clones official upstream at the pin. Without `-ReleaseCache`, it prepares source only; this host build currently needs a compatible Release cache. CMake is discovered on PATH or at the installed VS 2026 BuildTools location. This build path is presently Windows/VS 2026 specific. A self-contained fresh-engine compilation entry point remains a construction task.

Build outputs and logs live under `var/build/stalthos-runtime`. Each run creates a fresh directory under `var/reports/runtime`, with launch identity/executable hash, runtime receipt and a render-target screenshot after five seconds. The host copies the engine license beside its executable. Do not treat run-call counts as FPS; valid external frame-time capture is still required.

## Implemented foundation

`domain/foundation.hpp` separates responsive physical time from calendar scaling; provides body-relative steering, direct/destination/autonomous intent priority, separate body and camera stimuli, contact idempotence, deterministic region seeds, bounded retained identities, tree exceptions and hero-death continuity. Current heroes must use the explicit death transition; generic retirement/death cannot invalidate them. Six scenario groups cover these boundaries, including the independent review regressions.

The host uses the shared clock, direct authority, heading and initial hero identity. Other foundation cases are exercised by console scenarios and are not yet visible gameplay. State remains in memory: disk save/load, atomic recovery, lifecycle/ecology/streaming, real creature anatomy/animation, water, authored geography and game UI remain to implement. The blockout's analytical ground following is not collision/navigation acceptance.

See `../PROJECT-BUILD-PLAN.md` for the complete draft coverage and dependency order, and `../.clanker/current-work.md` for durable work state. Representative scene quality, moving images and target GTX 1080/Ryzen performance remain acceptance gates.


## Headless OpenCode dispatch

The user authorized `big-pickle` and `space-bunny` one-shot workers. Installed CLI: 1.18.35. Exact free-provider IDs: `opencode/big-pickle` and `opencode/space-bunny-free`. Specify `--agent build`: the configured default `orchestrator` is missing on this machine. Put the positional prompt before `--file`; that array option otherwise interprets the trailing prompt as another filename.

Task launchers under `.clanker/tasks/opencode-lod-review` and `.clanker/tasks/opencode-engine-review` use isolated checkouts under `var/worktrees`, `--pure`, JSON events and process results. Their process-local permissions allow read/glob/grep, deny edits and web tools, and put shell commands behind approval (`bash: ask`). Setting shell tools to deny reproduced a provider HTTP 403 free-tier error; switching to approval restored execution without approving any shell action. The brief explicitly requests read-only source analysis. Global OpenCode configuration was preserved. See upstream issue https://github.com/anomalyco/opencode/issues/53347 for matching permission-related reports; local events are the acceptance evidence.

These dispatch permissions describe a cooperative worker boundary, not an operating-system sandbox. Inspect actual final events and exit status before treating a worker as successful.
