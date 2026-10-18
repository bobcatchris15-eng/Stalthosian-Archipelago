# Actual archipelago atlas independent review — 2026-10-17

Read-only source reviewed in var/worktrees/archipelago-world-20261017.
No source edits, integration or commits performed.

Verified build with explicit installed CMake path (plain cmake initially unavailable
on PATH; retained initial diagnostic in ignored build.log):
C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe --build var/worktrees/archipelago-world-20261017/var/test_build/build --config Release
Actual output: MSBuild 18.6.3, test_archipelago_world.vcxproj -> Release/test_archipelago_world.exe; exit0.
Ran executable --atlas var/reports/world-population-20261017/atlas.json, exit0:
Successfully wrote coarse atlas JSON to: var/reports/world-population-20261017/atlas.json
ALL ARCHIPELAGO WORLD TESTS PASSED

Actual seed 20261017, 64x64 sampled grid, world bounds +/-14000m.
Matplotlib script reads actual exported elevation/moisture arrays. No terrain
reconstruction or hand-drawn islands. Plot inspected visually.

4-neighbor flood fill of elevation>0 finds four disconnected land components:
- Main Island: 146 nodes, estimated 28.840 km2, peak sampled124.8841m
- Rainridge: 79 nodes, estimated15.605 km2, peak159.6533m
- Stone Island:37 nodes, estimated7.309 km2, peak44.1702m
- Outer Reach:63 nodes, estimated12.444 km2, peak151.4594m
Each regional center maps to a distinct component, so these are actual separated
sampled islands rather than solely passing center-point elevation tests.
325/4096 nodes are land (7.9346%). Elevation range -323.742 to159.6533m.
Nearest sampled land-node separations from Main: Rainridge2222.2m,
Stone3142.7m, Outer2845.8m. These are not precise shoreline/strait widths.

Observed limitations:
- Grid spacing444.444m cannot resolve small islets, narrow straits or detailed shoreline.
- Node-count areas approximate sampled land; not exact clipped polygon areas.
- Rounded island masses are visible in this provisional field; cannot accept
  natural-history scenery or detailed geography from this coarse atlas.
- Actual nearest-grid center moisture: Main .5472, Rainridge .98,
  Stone .0609, Outer .1697. Region metadata stores .58/.88/.42/.28;
  Stone in particular is substantially drier than its metadata suggests.
- No hydrology, coastal vegetation, runtime lighting, streaming or FPS acceptance.

Outputs under var/reports/world-population-20261017/:
atlas.json (actual executable export), connectivity.json (independent flood fill),
world-atlas.png (labeled elevation/moisture figure), plot_atlas.py (reproducer),
build-verified.log, test.log, hashes.json (source/test/executable/export hashes).
