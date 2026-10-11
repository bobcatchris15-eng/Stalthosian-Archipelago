# The Stalthosian Archipelago

A Late Cretaceous-inspired fictional island archipelago and small-dinosaur life simulator. The target is a **full draft**: five playable lives, four islands, complete lifecycles and persistent world continuity. Wicked Engine is the selected native C++17/DX12 engine. The target machine is a GTX 1080 8 GB and first-generation Ryzen, with a 30 FPS floor still awaiting representative runtime evidence.

## Project layout

- `stalthos-runtime/`: Wicked diagnostic host, engine-independent domain foundations and native scenarios. Read its README for build and run commands.
- `stalthos-toolkit/`: reproducible asset pipeline, schemas, command status envelopes and offline tests. Begin with `docs/toolkit-architecture.md` and `docs/toolkit-pipeline.md`.
- `PROJECT-BUILD-PLAN.md`: full draft coverage, dependency order and acceptance requirements.
- `.clanker/`: durable task briefs, review records and continuation state.
- `var/`: ignored tools, engine source/cache, builds, worktrees and runtime evidence. These are prepared locally rather than committed binaries.

Planning input belongs to the separate read-only repository [Rauken-Archipelago](https://github.com/bobcatchris15-eng/Rauken-Archipelago). Clone it into `Rauken-Archipelago/` when preparing a new checkout; its documentation is not vendored or edited here.

## Current evidence

The initial Release host builds and imports toolkit-produced plant geometry. Six native domain scenario groups pass. The toolkit starting integration passed 1922 tests with two skips. These are construction and diagnostic results; the full draft, material quality, animation and representative target-hardware frame-time acceptance remain open.

The project uses `main` as its synchronized branch. Local workers use isolated `codex/` branches, and remote Jules results are reviewed and tested before integration. Only coherent validated source and task records are published; tool downloads, build caches and raw local runtime captures remain under `var/`.

## Toolkit setup

Install the Python package with `python -m pip install -e ./stalthos-toolkit`. The local `stalthos` command must resolve this checkout, rather than a retired worker directory. Portable optimization packages are pinned: gltfpack 1.3 and Compressonator 4.5.52. Cache the official release ZIPs under `var/tools/downloads/`, then run `./stalthos-toolkit/scripts/Prepare-Tools.ps1` and dot-source `./stalthos-toolkit/scripts/Use-Tools.ps1`. The normal PowerShell pipeline wrapper activates verified packages automatically. Setup checks archive and executable hashes and preserves mismatched existing installations. Optional missing tools remain visible in `stalthos doctor`.

Official portable archives: [gltfpack Windows 1.3](https://github.com/zeux/meshoptimizer/releases/download/v1.3/gltfpack-windows.zip) (save as `gltfpack-windows-1.3.zip`) and [Compressonator CLI 4.5.52](https://github.com/GPUOpen-Tools/compressonator/releases/download/V4.5.52/compressonatorcli-4.5.52-win64.zip).

## Fourth construction checkpoint

Wicked imports the animated Smalltrotter draft alongside eight authored shrubs. STL3 persists current hero lifecycle age/species/stage with STL1/STL2 read compatibility; actual separate-process save/load and terminal death probes pass. Native CTest passes 4/4. Foot contact/low-tail clearance, full gameplay coverage and external 30 FPS acceptance remain unfinished. See `.clanker/current-work.md` and the fourth integration receipt for test status and the three active Jules tasks. Worker selection now uses the lowest capable model/effort; Jules has a separate three-task remote pool with a 100/day ceiling.

The walk-cycle toolkit changes are currently a local validation candidate. Native actor support accepts an external animated GLB; final toolkit suite completion is required before publishing the generator changes.
