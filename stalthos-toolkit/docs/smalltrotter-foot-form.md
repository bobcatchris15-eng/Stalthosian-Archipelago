# Smalltrotter Foot & Ankle Form Receipt

Status: visual blockout refinement; fictional anatomy and proportions are **speculative**.
This receipt records the distal limb and foot blockout refinements for `creature.smalltrotter.st1` (independent juvenile, seed 42).

## Visual Direction and Rejection Context

Parent commit `58c1865` was visually rejected for final promotion based on `var/smalltrotter-connected-verified-asset/preview/.../neutral_090.png`:
- **Black Cylindrical Ankle Cuffs**: The distal shank below z=0.24 was classified by material heuristics as dark keratin (`MAT_Smalltrotter_Keratin_PBR`), creating an abrupt, artificial black boot collar midway down the lower leg.
- **Flat Dark Disc Footpads**: The feet were constructed from oversized lateral ellipsoids (`1.55 * leg_r` wide) and massive dark toe spheres in the detail mesh, presenting a flat, disjoint shoe/boot silhouette.
- **Abrupt Ankle Transition**: The limb sweep terminated with a flat endcap at the ankle landmark before meeting the foot disc.

Guided by the primary visual reference (`references/creatures/smalltrotter/user-primary.png`), the distal limbs require continuous, stout sauropod proportions where mottled ochre hide skin extends seamlessly down to the ground, blending into compact fleshy feet equipped with subtle, blunt, ground-proximal toenails.

## Changes Implemented

1. **Continuous Fleshy Skin Across Distal Limbs and Feet (`creature.py`)**:
   - In `creature.py`, after calling `CREATURE_MAT.assign_smalltrotter_export_materials()`, all distal limb and foot polygons on `body_obj` previously assigned to material slot 2 (`MAT_Smalltrotter_Keratin_PBR`) are reassigned to slot 0 (`MAT_Smalltrotter_Hide_PBR`).
   - This eliminates the 906 dark keratin faces on `body_proxy` (`dark_foot_face_count: 0`), allowing the mottled ochre hide skin to flow uninterrupted from the haunch/shoulder down to the toes.

2. **Stout Distal Limb Sweep & Embedded Ankle Transition (`creature.py`)**:
   - Extended the Catmull-Rom `surface_path` from 6 to 7 control points, adding a distal foot root target (`ankle + (foot - ankle) * 0.55`) so the limb tube sweeps smoothly past the ankle into the upper foot volume.
   - Refined the distal radius profile to maintain stout, columnar sauropod proportions instead of tapering to a narrow stilt:
     - Forelimb distal radii: depth `[1.05, 0.88, 0.92, 0.98] * leg_r`, width `[0.95, 0.82, 0.86, 0.92] * leg_r`.
     - Hindlimb distal radii: depth `[1.12, 0.90, 0.94, 1.00] * leg_r`, width `[0.98, 0.84, 0.88, 0.94] * leg_r`.
   - The endcap of the limb tube is fully embedded inside the fleshy foot mass, eliminating visible cap seams.

3. **Compact Fleshy Foot Mass & Fleshy Digital Cushions (`creature.py`)**:
   - Replaced the oversized `1.55 * leg_r` disc with a compact fleshy foot ellipsoid on `body_obj` (`0.46 * foot_length` long, `1.12 * leg_r` wide, `0.72 * leg_r` tall) that closely matches distal limb width.
   - Added subtle fleshy digital cushions on `body_obj` (`0.16 * foot_length`, `0.30 * leg_r` width), sharing the continuous mottled hide material.
   - Both foot and digital cushions terminate exactly at `foot_floor = 0.004 m`, maintaining the required floor minimum.

4. **Subtle Blunt Toenails in Detail Mesh (`creature.py`)**:
   - Replaced the large dark toe balls on `detail_obj` with subtle, compact blunt toenails (`0.065 * foot_length` long, `0.13 * leg_r` wide, `0.12 * leg_r` tall).
   - Because `detail_obj` receives `MAT_Smalltrotter_Keratin_PBR`, the toenails render with dark fibrous keratin, peeking out subtly at the ground edge of each fleshy digital lobe as seen in `user-primary.png`.

5. **Strict Landmark and Contact Gate Invariance (`creature.py`)**:
   - Joint graph (`hip`, `knee`, `ankle`, `foot`), bone lengths (`leg_upper_m = 0.27`, `leg_lower_m = 0.234`), stance height (`0.95 m`), and neutral pad bottom (`0.004 m`) are preserved without change.
   - `pad_vertex_indices` accurately captures all foot and digital cushion vertices on `body_proxy`.
   - Dynamic walk cycle contact validation passes with zero contact gate or foot lock violations (measured max excursion `0.0117 m` within the `0.02 m` gate).

6. **Unit Test Coverage (`tests/test_reference_anatomy.py`)**:
   - Added `test_tapered_limb_seven_point_distal_profile_preserves_closed_manifold()` verifying the 7-point sweep forms a closed 2-manifold without join caps. All 11 tests pass offline in 0.10s.

## Verification Artifacts & Evidence

- **Workspace**: `E:\Stalthosian Archipelago\var\smalltrotter-foot-form-asset`
- **Seed**: 42, `independent_juvenile`, Blender 5.2.0 LTS
- **Artifacts**:
  - `sources/.../creature.smalltrotter.st1.independent_juvenile.blend`: `SHA256: 71E7602BDA8ACB1A88E09CA394A44490A8E8C879AF26D5BBF4551039C2B5B3A0`
  - `runtime/.../creature.smalltrotter.st1.independent_juvenile.neutral.glb`: `SHA256: A59932456439424E9E3724D3505CFA0F6419585E78372BBE1F45F167C653C48F`
  - `runtime/.../creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb`: `SHA256: 22C305CB7A4FCD078D2E2FAF2D10E1C787F67997B34847F385E87C62DED5365E`
  - `preview/.../creature.smalltrotter.st1.independent_juvenile_neutral_090.png`: `SHA256: A6725704ACA83C4DB2AF478BFF6ED956F26C17B195BB1F17B9E0093C6E4EAB1D`
  - `preview/.../creature.smalltrotter.st1.independent_juvenile_neutral_000.png`: `SHA256: CBBD417056464717857ABAA537C02599592FDAEF4D74CB1B536BEFED9F99E797`
- **Metrics**:
  - `neutral_pad_bottom_m`: `0.004` (source pads: `0.004` across all 4 limbs)
  - `dark_foot_face_count`: `0` (reassigned to continuous hide)
  - `ventral_face_count`: `170`
  - `triangles`: `16236` (budget < 50,000)
  - Rig validation: `passed: true`, `loop_passed: true`, `root_motion_passed: true`, `geometric_contact_passed: true`, `touchdown_foot_lock_passed: true`, `glb_animation_passed: true`
  - Intermediate warning: `[rig_creature] smalltrotter_bristle_ridge_blockout: 1260 vertices carry no bone weight` (expected intermediate auto-weight notice for rigid bristle proxy, unassigned skin metric 0).

## Visual Inspection Summary

Inspection of `neutral_090.png` and `neutral_000.png`:
- The black boot cuffs and flat dark discs are completely gone.
- Legs present as sturdy, columnar sauropod appendages flowing with mottled ochre hide skin directly into compact rounded fleshy feet.
- Subtle blunt dark toenails peek out at the ground margin at the toes.

## Honest Remaining Limitations

- Geometry remains an anatomical blockout of interpenetrating manifold shells (body, detail, eyes, bristles), not a single retopologized mesh.
- Integument skin wrinkles and scales are procedural normal/albedo textures rather than sculpted micro-geometry.
- Fictional anatomy is speculative based on the single 2D user reference image.

Parent verification: independently rebuilt frozen4218e30 in fresh var/smalltrotter-foot-verified-asset; build/rig/render all oktrue exit0 and artifacthashes match. Fullroot offline suite1986passed2skipped597.13s; sourcehashes frozen throughrun. Only trailingblankEOF cleanup afterward; focused11passed. Fresh parent neutral090 confirms cuffremoval. Native import/idle withnewGLB in20261012-115956-477ec565 loads successfully. Gait cadence, torso/foot intersections, facial/bristle appearance and fullreference parity remain unaccepted.
