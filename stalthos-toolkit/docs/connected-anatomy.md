# Smalltrotter Connected Anatomy Receipt

Status: visual blockout iteration; fictional anatomy and proportions are **speculative**.
This receipt records the corrected connected limb sweeps and torso-integrated shoulders/haunches for `creature.smalltrotter.st1`.

## Intent and Context

Commit `43ed522` was visually rejected after inspection of `creature.smalltrotter.st1.independent_juvenile_neutral_090.png`:
- Conspicuous round shoulder/hip discs were produced by independently visible `add_girdle_transition` ellipsoid overlays.
- A hard cream-ring seam appeared where protruding limb root endcaps caught the ventral material shader assignment.
- The appendages presented cylindrical bootlimb profiles with insufficient distal taper toward the ankles.

Guided by the primary visual reference (`references/creatures/smalltrotter/user-primary.png`), this update corrects the anatomical blockout while preserving all joint graph bone lengths, footpads, and contact gates.

## Changes Implemented

1. **Removed Girdle Ellipsoid Overlays (`creature.py`)**:
   - Removed the lateral `REF.add_girdle_transition()` ellipsoid calls in `build_body()`, eliminating the spherical bulging discs on flanks and shoulders.
   - Retained the `add_girdle_transition()` helper in `reference_anatomy.py` to maintain unit contract and test coverage.

2. **Embedded Limb Roots & Elimination of Ring Seams (`creature.py`, `reference_anatomy.py`)**:
   - Shifted `embedded_root` control points deeper inside the torso volume (`side * 0.35` rather than `side * 0.60`).
   - Narrowed proximal root radii (`1.35 * leg_r` fore, `1.48 * leg_r` hind) so that the first ring and its cap face remain fully enclosed within the trunk mesh, preventing topcaps from poking through or catching ventral material classification.
   - Fixed a scalar Catmull-Rom clamping bug in `add_tapered_limb()` where negative coordinate values on hindlimbs (negative X) and right limbs (negative Y) were previously clamped to `1e-5`. Corrected `front_right` limb classification to `is_fore=True`.

3. **Natural Juvenile Limb Taper (`creature.py`)**:
   - Replaced cylindrical profiles with continuous tapers reflecting the juvenile sauropod proportions in `user-primary.png`:
     - Forelimb: depth radii `[1.35, 1.60, 1.28, 1.02, 0.82, 0.68] * leg_r`; width radii `[1.10, 1.25, 1.08, 0.90, 0.76, 0.62] * leg_r`.
     - Hindlimb: depth radii `[1.48, 1.82, 1.42, 1.10, 0.86, 0.70] * leg_r`; width radii `[1.18, 1.32, 1.15, 0.94, 0.78, 0.64] * leg_r`.
   - The limb sweeps now taper toward the ankles. Separate foot pads, torso intersections and hard material boundaries remain visibly apparent in parent review; this is an incremental blockout correction, not a seamless final skin.

4. **Preserved Contracts & Contact Gates**:
   - Exact authored joint records (`hip`, `knee`, `ankle`, `chain`, `spine`, `neck`, `tail`, `head`) preserved in serialized joint dictionary.
   - Authored bone lengths (`leg_upper_m`, `leg_lower_m`) preserved.
   - Foot pad vertex indices strictly preserved (`body.verts[pad_start:]`).
   - Ground contact height remains exactly 0.004 m (`neutral_pad_bottom_m`).
   - Dynamic walk cycle contact validation passes with zero contact gate or foot lock violations (within 0.02 m tolerance).

## Verification

- **Focused Tests**: `PYTHONPATH=src python -m pytest tests/test_reference_anatomy.py` -> **10 passed in 0.08s**. No tests weakened.
- **Headless Blender Pipeline (seed 42, independent_juvenile)**:
  - `build_creature` -> `exit_code: 0`, `ok: true`. Body proxy reduced from 5474 to 4682 vertices; ventral faces reduced from 233 to 170.
  - `rig_creature` -> `exit_code: 0`, `ok: true`. Loop, root motion, geometric contact, and touchdown foot-lock validations all passed.
  - `render_turntable` -> `exit_code: 0`, `ok: true`. Produced 4 neutral angle review renders (0°, 90°, 180°, 270°).

## Visual Inspection & Comparison

- Compared `var/rejected-43ed522/creature.smalltrotter.st1.independent_juvenile_neutral_090.png` against new `var/smalltrotter-connected-mesh-asset/preview/.../neutral_090.png`:
  - Bulging lateral discs at hip and shoulder are eliminated.
  - Pale cream ring seams on proximal limbs are eliminated.
  - Cylindrical bootlimb columns replaced by anatomical taper to ankle.

## Honest Remaining Limitations

- Mesh topology is procedural blockout geometry (interpenetrating manifold volumes), not a single retopologized/subdivision skin mesh.
- Facial detail (eyes, nostrils, mouth lining) remains a blockout approximation; eyes are non-articulated beads.
- Scaled dermis and skin wrinkles are shader/normal approximations, not sculpted micro-geometry.
- Bristle crest is procedurally generated fibers; dynamic grooming and collision are not modeled.
- Full runtime locomotive variety, turning banking, and terrain adaptation await runtime gameplay integration.

## Parent reproduction — 2026-10-12

Parent rebuilt frozen isolated commit 58c1865 from its toolkit directory with PYTHONPATH=src into a new ignored workspace, var/smalltrotter-connected-verified-asset. Build, rig and turntable each returned ok=true/exit_code=0. Fresh neutral_090 inspection confirms the lateral disc overlays are absent. Native moving capture var/reports/runtime/20261012-113703-3ea9fd55 and idle capture 20261012-113847-0aa96827 load this exact new GLB. Walk phase follows distance and idle holds phase zero, but the short authored stride produces roughly twelve cycles per second at the present host pace. Movement quality is unaccepted. The intermediate automatic-weighting warning reports 1260 bristle vertices, but the subsequent explicit feature bind produces zero unassigned vertices and passes final feature-skin metrics; the warning is stale and must not be treated as final missing weights. Bristle appearance in motion remains unaccepted. The full offline regression result is recorded separately by the parent receipt.
