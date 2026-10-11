# Animated actor host

The native host accepts an optional `--creature <glb>` input (PowerShell: `Run.ps1 -Creature <glb>`). It imports the GLB into a fresh Wicked `Scene`, then checks for at least one mesh object, one armature/skin and one animation with at least one TRS channel before instantiating it. Missing or neutral-only animation data fails with a diagnostic error. Without `--creature`, the existing box marker remains the fallback and is explicitly diagnostic.

Wicked's importer performs its own glTF axis conversion. The authored Smalltrotter is longitudinally +X in Blender and glTF uses Y-up; the host preserves that importer-created root and its descendants, then applies heading and the provisional +X-to-host-+Z facing rotation at a separate hero motion parent. The imported rig is not cleared or translated during ticks. The supplied juvenile GLB measured approximately 1.572 x 0.496 x 1.224 m. A provisional sole offset of 0.652438 m places the authored rear sole plane at analytical terrain height; the low tail bound extends below that plane, so tail clearance and foot planting remain unaccepted pending an authored fix.

All imported animation components are looped. While the walking input is active, the host plays them; when it stops, the host pauses them. Animation receipt evidence tracks timer min/max across loop wrap, playing frames, unique targeted TRS channels, and maximum local pose change on animated targets. Screenshots are captured at early and late distinct phases. These measurements establish native playback and pose change only; they do not establish foot contact, locomotion quality, final visuals, or target-hardware performance.

The optional `--calendar-scale 0..10000` (`Run.ps1 -CalendarScale`) feeds one scaled `TimeStep` from `SimCore::advance` to lifecycle advancement. Current hero lifecycle is restored from STL3 when present, and legacy/absent lifecycle starts at age zero for the selected default species (Smalltrotter). Current age, stage, transitions, saved lifecycle values, and hero alive/death-known state are included in the runtime receipt. Reaching Dead calls `world.heroDied()`, pauses all actor animations, and stops host movement. Lifecycle is saved only when a current hero exists. Save data still does not cover needs, bonds, reproduction, or full animated pose.

Example feasibility run:

```powershell
./stalthos-runtime/scripts/Run.ps1 -Duration 6 -Creature 'path/to/creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb' -DiagnosticWalk -CalendarScale 1000
```

The Run script writes `runtime-receipt.json`, `draft-host.png`, `motion-early.png`, and `motion-late.png` into a fresh ignored `var/reports/runtime/<id>/` directory. Acceptance remains false in the receipt.

Reverse movement also steps the provisional clip; reverse foot mechanics are not accepted. The host continues to reject no-current-hero loads; post-death continuation and ordinary successor selection remain a required full-draft capability.
