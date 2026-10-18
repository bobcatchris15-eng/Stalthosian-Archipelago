# General project state — world construction priority

Updated 2026-10-17. Direct user steering: prioritize building the world, simulations,
systems and populations. Population meshes/assets are best effort. Reuse available
assets as replaceable presentation; Smalltrotter/model/HeroViewer polish is off the
critical path unless it blocks the world. Full draft remains five playable lives
and four islands; the reference watershed is a connected construction step.

## Active work

- Native bounded fauna population module: codex/fauna-population-20261017,
  var/worktrees/fauna-population-20261017. Owns new domain/fauna_population.hpp,
  tests/fauna_population.cpp and its doc. Root owns engine/main.cpp, renderer
  adapter and CMake integration. Public ActorView/Population API is in its brief.
- Jules environment module session3306157609621882461: owns new
  domain/world_environment.hpp, tests/world_environment.cpp and doc. Collect once
  into isolation when ready; inspect/compile/review before root integration.
- Root connects autonomous region actors to actual dry habitat paths, shared
  food/water sites and existing creature assets. Rendering remains replaceable.

## Validated checkpoint immediately before world work

Toolkit2073 passed/2 skipped (627.89s); focused plant64; Wicked Release and CTest24/24.
Exact Smalltrotter seam824 pairs now succeeds on a derived base cage, closed and
reopened with other meshes preserved; source SHA256 b987e7d7...d901de15350 unchanged.
Receipt source/output aliases (including hardlinks) are refused before writes.
Source-derived UV/material/rig work is deferred behind world construction.

Fresh conifer stage GLBs reproduced prior candidate bytes with six bindings each;
20 actual placements, zero ground fallbacks. Canopy reads sparse and remains
provisional; no asset-polish gate for population work. Existing asset files remain.
Crossfade implementation compiled/replayed with225 shared channels, one setup
analysis,34 sampled blend frames/four interior captures and signed phase error
4.44e-16s. Death frozen to numerical tolerance; no-creature fallback passes.
No gait/foot locking/visual/FPS acceptance. See tasks/continuation-20261017/receipt.json.

## Backlog and preservation

31 new Jules sessions retrieved:26 completed,4 awaiting feedback,1 failed;
30 patches. Three bounded corrections integrated (optics lookup, DOI normalization,
Windows banner regression);3 rejected-as-written;24 deferred;1 no-diff already
addressed. Inventory tasks/jules-backlog-20261017. GitHub had27 open PRs; no remote
states changed. Do not repeatedly retrieve old collected sessions. New environment
session above is the only newly dispatched Jules task.

Rauken-Archipelago is read-only. Preserve unrelated untracked receipts/logs/object
files. Use isolated ownership, lowest confidently capable model/effort and no paid
API fallback. Validated publication to origin/main is authorized, without force,
releases or deployment. Check HALT before dispatch. Full draft remains incomplete.
