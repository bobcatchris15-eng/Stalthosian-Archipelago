# Evidence

Release configuration was built with the checkout-local cache junction and the pinned engine source:

`./stalthos-runtime/scripts/Build.ps1 -Source 'E:\Stalthosian Archipelago\var\vendor\WickedEngine' -CacheDirectory <this-checkout>/var/vendor/wicked-release-cache`

The final incremental build after the parent supplied the reviewed lifecycle persistence header completed and produced `var/build/stalthos-runtime/Release/Stalthos.exe`.

Run 1, using `Run.ps1 -Duration 4 -Creature <Smalltrotter walk-cycle GLB> -CalendarScale 1000 -DiagnosticWalk -SaveState <var/saves/actor-host-stage.stl>`, produced receipt `var/reports/runtime/20261011-172249-ea4eba59/runtime-receipt.json`: 1 mesh, 1 skin, 1 animation, 222 transform channels; local animated-target pose delta 0.150663; 1,130 frames with animation playing; timer range 0.002714–2.138928 seconds; lifecycle transitioned juvenile then adult; hero stayed alive.

Run 2 loaded that save in another process with `Run.ps1 -Duration 1.5 -Creature <same GLB> -CalendarScale 10 -DiagnosticWalk -LoadState <actor-host-stage.stl>`. Receipt `var/reports/runtime/20261011-172258-68626fc5/runtime-receipt.json` recorded `lifecycle_snapshot_loaded=true`, loaded age 3423.0714045697823 seconds/stage 2, and advanced to age 3431.3576706267486 while alive.

Run 3 used 17 seconds at scale 1000 with the creature and a save path. Receipt `var/reports/runtime/20261011-172306-9647dd73/runtime-receipt.json` recorded stages juvenile/adult/old/dead, final dead stage, `hero_alive=false`, `hero_death_known=true`, and over 4,600 animation-playing frames before the terminal transition. This run validates host-side stop/death truth for this short diagnostic pacing; its saved state has no current hero, and the host intentionally rejects loading snapshots without a live hero.

Early and late images from run 1: `motion-early.png` SHA-256 `cf230106c6b6eea1b87010abcc2314324d3f27ce8002ba04aa5adbf8d3e98b5b`; `motion-late.png` SHA-256 `67d8954aa82b7bd1a044de354eb6e0a7ed0984dc4c25afe67b916d66ea6fa228`. Both are under the same run directory above. Screenshots show the imported gray proxy creature in the diagnostic island scene.

Visual review: actor is visibly oriented along host +Z after the motion-parent yaw offset and its imported hierarchy remains under that parent. A provisional 0.652438 m sole-plane offset was sourced from gait-work bounds; the low-tail bound lies below that plane, so foot planting/tail clearance remain unaccepted. No target-hardware frame-time measurement or final visual acceptance was performed.

Final provenance hashes from the completed actor-host run:

- Input GLB `E:\Stalthosian Archipelago\var\worktrees\luna-walk-cycle\var\luna-walk-cycle\runtime\creatures\creature.smalltrotter.st1\creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb`: SHA-256 `ba5245ce725a4488f04c5ca6792202889be589b46cdfe537083e6577193d8197`.
- Host executable `var/build/stalthos-runtime/Release/Stalthos.exe`: SHA-256 `ad90c6eeaf9f1eb165952e692a7879db3db9f52dd116b1ce00dfee92a919e4a3`.

Across the two process continuity proof, lifecycle age loaded at `3423.0714045697823` seconds and ended at `3431.3576706267486` seconds, a `8.286266056966`-second calendar advance during `0.828626605697` physical seconds at scale 10. Both loaded and final stage were Adult (`2`); the second receipt reported the lifecycle snapshot present.
Final host executable SHA-256 after the terminal-input guard: $hash.
