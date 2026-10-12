# Smalltrotter neutral support pose revision

## Scope

This changes only the quadruped proxy's neutral body-axis pose. It does not claim
anatomical validation, locomotion acceptance, or photorealistic presentation.
The authored upper and lower leg segment lengths remain unchanged.

## Pose construction

`blender/scripts/anatomy_pose.py` computes independent Z corrections at the
existing rear (`t=0.16`) and fore (`t=0.80`) support anchors. It accounts for
upper-leg spread, the existing lower-segment direction, stance foot drop and
pad thickness, then interpolates the correction along the trunk while holding
it at the axial ends. The builder applies this correction to spine, neck/tail
anchors, leg hips and surface details without changing the world axes or joint
names. Tail rise follows the declared positive rise direction to preserve tail
clearance after lowering the support body.

## Measurements

Actual Blender build results include analytic neutral pad-bottom height,
body-offset values, original segment lengths and the minimum tail mesh Z. These
are source geometry measurements for the generated proxy. They do not measure
an armature-deformed gait or establish a visual acceptance gate.

## Review evidence

The task receipt records the Blender 5.2 seed-42 independent-juvenile build and
broadside before/after captures under the worktree's ignored `var/` directory.
Both images use the same deterministic render entrypoint and camera setup; the
before is the 90-degree turntable output and the after is a bounded one-angle
render of the final source master.
