# Smalltrotter reference-led anatomy blockout

Status: provisional visual blockout; fictional anatomy and every proportion here are **speculative**. This pass is scoped to the Smalltrotter source proxy and preserves the existing neutral support anchors and joint-chain interface.

## Reference and decisions

The sole visual input is `references/creatures/smalltrotter/user-primary.png`, with `visual-direction.md` as its written guide. The profile is selected by exact asset id (`creature.smalltrotter.st1`); other creature IDs retain the existing builder path. It shapes a deeper, longer torso with shoulder and pelvic masses; gives the neck a long upright curve and the head a blunt cranium, muzzle, jaw, brow, and eye volumes; thickens the limbs; rounds the feet and adds three toe/claw volumes; lowers and thickens the tail root; and places an irregular seed-driven bristle ridge over nape, back, and tail.

The helper in `src/stalthos_toolkit/blender/scripts/reference_anatomy.py` is bpy-independent. It applies a per-asset copy of shape parameters and offers MeshBuilder-compatible ellipsoid, curve-sampling, and irregular-ridge operations. Geometry remains in `creature.py`; no core, rig, gait, definition, or material-helper changes are part of this pass. Existing `head`, `limbs`, `neck`, `spine`, and `tail` joint records remain present. The rounded hind-foot support vertices were confirmed by the existing rig selector and toe-weight assignment.

## Review and limits

A baseline build and the revised build both ran in Blender 5.2.0 LTS with seed 42. The revised build rendered a broadside and three-quarter view, each compared side-by-side with the supplied image. The blockout improves the long neck, compact head volumes, torso mass, low tail, rounded foot supports, and bristle silhouette over the original box-foot/flat-head proxy. It remains visibly primitive: limbs and torso are separate hard-edged masses, the neck/chest intersection has a seam, the ridge still reads as spikes, face and skin shading are unfinished, and the feet/claws need refinement. The render uses a neutral studio backdrop and is not a scene or runtime acceptance capture.

`rig_creature` exported neutral and walk-cycle GLBs and validated the animation container, but its report marked geometric ground contact and touchdown foot lock as failed. Running the same command on the unmodified seed-42 baseline produced the same warning. This pass did not alter the rig or weaken its checks. The failure remains an existing rig/contact limitation; successful export is not evidence of a passing walk cycle.

No paleontological reconstruction, locomotion quality, runtime import, production shading, or player-facing visual acceptance is claimed. This is a reference-driven anatomy draft for iteration.

## Reproduction

From `stalthos-toolkit/`:

```powershell
$env:PYTHONPATH = 'src'
python -m stalthos_toolkit.cli build_creature --definition species/creatures/smalltrotter.yaml --seed 42 --workspace '..\var\anatomy-final' --blender 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
python -m stalthos_toolkit.cli rig_creature --definition species/creatures/smalltrotter.yaml --seed 42 --workspace '..\var\anatomy-final' --blender 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
python -m pytest -q tests/test_reference_anatomy.py tests/test_anatomy_pose.py
```

The recorded focused offline test result is **8 passed**. The build command returned `ok=true`, exit code 0. The rig command returned `ok=true`, exit code 0, with the contact failures described above. The two reviewed local comparisons are in ignored workspace output at `var/anatomy-final/broadside-comparison.png` and `var/anatomy-final/threequarter-comparison.png`.
