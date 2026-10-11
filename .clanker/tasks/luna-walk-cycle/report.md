# Smalltrotter walk-cycle task report

Outcome: partial proxy locomotion export. The final rig command produced a distinct neutral GLB and a looping walk-cycle GLB from the YAML `animation_contacts.walk_cycle` entry. The loop boundary is exact and the exported GLB validates with one animation and 222 channels. The clip is 2.0 seconds at 24 fps; the source Blender proxy faces +X and exports with Y-up.

The authored gait does not pass terrain contact. The actual armature-deformed hind foot-pad bottom vertices were measured over the clip. Across 9 nominal-stance samples per side, their z gaps remain 0.646–0.665 m from the current z=0 plane, against the authored 0.02 m tolerance. In-place horizontal stance slip reaches 0.104 m (left) and 0.079 m (right). The clip has no actor stride translation, so these are terrain-contact-height and in-place slip diagnostics; they do not establish planted feet.

The generated pads are embedded in `body_proxy` (8 vertices per hind pad). In neutral, rear pad bottom is z=0.652438 m and front pad bottom is z=0.671890 m. The lowest remaining body point is the tail at z=0.469616 m, so lowering the whole asset to put a pad at z=0 would drive the tail below ground. No vertical offset was applied to hide that clearance conflict. The neutral GLB remains animation-free.

The first Blender attempt failed because Blender 5.2's layered `Action` does not expose legacy `.fcurves`; the compatibility code was removed, boundary keys remain equal, and the subsequent export has valid animation channels. See `initial-failure.txt`.

Verification: `python -m pytest -q stalthos-toolkit/tests/test_blender.py stalthos-toolkit/tests/test_gait.py` passed 69 tests in 132.29 seconds. A separate final `build_creature` plus `rig_creature` run used the worktree's absolute `PYTHONPATH` and Blender 5.2.0. Native playback import/render, target-hardware performance, visual gait quality, foot IK, and tail-clearance acceptance remain outside this evidence.

Animated GLB: `var/luna-walk-cycle/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb`
Neutral GLB: `var/luna-walk-cycle/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.neutral.glb`
Motion/contact report: `var/luna-walk-cycle/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.json`
