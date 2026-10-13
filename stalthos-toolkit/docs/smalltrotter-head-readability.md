# Smalltrotter Head Readability & Anatomy Refinement Receipt

Status: visual blockout refinement; fictional anatomy and proportions are **speculative**.
This receipt records the cranial, muzzle, eye, and throat blockout refinements for `creature.smalltrotter.st1` (independent juvenile, seed 42).

## Visual Direction and Rejection Context

Prior renders (e.g. `neutral_090.png`) exhibited several anatomical blockout deficiencies:
- **Segmented Head and Snout**: Constructed from disjoint, constricted ellipsoids (`head_centre`, `muzzle_centre`, `jaw_centre`) creating an artificial waist/dip and multi-lump silhouette.
- **Angular Exposed Eye Blobs**: Buried under protruding brow lumps, presenting only as squashed dark crevices or angular slits.
- **Dark Patchwork Across Face**: Smart-UV island cuts across the head sampled dark mottle regions of the hide texture map, splattering random polygon-shaped dark blotches across the snout and cranium.
- **Abrupt Throat Termination**: Pale ventral skin (`MAT_Smalltrotter_Ventral_PBR`) stopped midway up the neck, leaving the upper neck, throat, and lower jaw dark and blotchy.
- **Protruding Wart Nostrils**: Nostril blockouts were oversized dark spheres projecting awkwardly off the snout tip.

Guided by the user's primary visual reference (`codex-clipboard-0ed107a6-e5c9-456d-977e-18bcc1d9a265.png`), the Smalltrotter head requires:
- One coherent, gently tapered head volume with a gentle cranial dome and a broad, rounded snout.
- Readable, prominent, warm golden-amber lateral eyes with centered pupils and glossy corneal curvature, cradled in smooth orbital eyelids.
- Subtle, flush nostrils positioned naturally on the dorsal snout tip.
- Continuous pale cream/buff throat running from the chest up the front of the neck and under the jaw and chin.
- Clean, warm ochre facial hide without fragmented dark patchwork.

## Changes Implemented (`creature.py`)

1. **One Coherent Gently Tapered Head Sweep (`_elliptical_tube` & Snout Cap)**:
   - Replaced disjoint cranium, muzzle, and jaw ellipsoids with a continuous 5-station elliptical sweep (`head_sweep_path`) extending from the neck junction to the snout tip.
   - Cross-sections taper gently:
     - Neck junction: width `0.84 * neck_r[-1]`, depth `0.96 * neck_r[-1]` (seamlessly embedded inside the distal neck tube).
     - Cranium: depth `0.50 * hd`, width `0.46 * hd` with a gentle dorsal cranial dome (`cranium_dome`).
     - Mid-skull / orbital region: depth `0.42 * hd`, width `0.42 * hd`.
     - Anterior muzzle: depth `0.35 * hd`, width `0.38 * hd`.
     - Snout tip: depth `0.27 * hd`, width `0.35 * hd`.
   - Added a broad rounded muzzle cap ellipsoid (`snout_tip`) rounding off the snout tip (`head_len * 0.18` x `hd * 0.36` x `hd * 0.27`), delivering a sturdy, rounded sauropod snout.

2. **Readable Warm Lateral Eyes & Smooth Orbital Contours**:
   - Placed large, expressive lateral eye domes (`head_len * 0.10` x `hd * 0.075` x `head_len * 0.10`) positioned proudly on the lateral skull face.
   - Added smooth, anatomical eyelid/orbital socket cushions on the `body` mesh (`head_len * 0.14` x `hd * 0.09` x `head_len * 0.13`), eliminating harsh angular brow lumps.
   - Authored a deterministic warm golden-amber iris texture map (`MAT_Smalltrotter_Eye_PBR`) with a crisp centered pupil (`col = (0.015, 0.012, 0.010)`), radial iris striations (`col = (0.46, 0.29, 0.11)` linear sRGB), dark limbal ring, and glossy cornea roughness (`0.08`).
   - Projected lateral eye UVs to display the centered pupil and radiant warm iris directly to broadside and three-quarter cameras.

3. **Subtle Flush Nostrils**:
   - Reduced nostril dimensions to low-profile flush ovals (`head_len * 0.022` x `hd * 0.014` x `head_len * 0.018`) on the dorsal-anterior snout.
   - Mapped nostril UV coordinates cleanly into the dark pupil zone of the feature map, presenting subtle, natural narial depressions without protruding black lumps.

4. **Continuous Pale Ventral Throat & Chin Underside**:
   - Extended ventral material assignment (`MAT_Smalltrotter_Ventral_PBR`, slot 1) to cover all forward/ventral polygons along the upper neck and the entire underside of the throat and lower jaw (`ventral_face_count` increased from 170 to 578).
   - Gives the head and neck the characteristic pale cream/buff throat seen in the primary reference image.

5. **Elimination of Dark Patchwork on Head Hide**:
   - Re-mapped dorsal head and snout hide polygons into the uniform warm ochre domain of `ST_Smalltrotter_hide_BaseColor_s...` (`[0.46, 0.54]`), preserving fine micro-pore variation while eliminating random dark mottle cuts across the face.

6. **Strict Invariance of Skeleton, Joint Graph, Body Proportions, and Gait**:
   - Exactly retained `skull_path` points (`[neck_path[-1] - tangent * head_len * 0.12, head_centre, muzzle_centre, muzzle_centre + tangent * head_len * 0.34]`).
   - Retained `joints["head"]`, `joints["neck"]`, `joints["spine"]`, `joints["tail"]`, and `joints["limbs"]`.
   - Authored bone lengths (`upper = 0.27`, `lower = 0.234`), stance height (`0.95`), and neutral pad bottom (`0.004 m`) are unchanged.
   - Dynamic walk cycle contacts pass all gates with zero contact or foot lock violations (`max_excursion_m = 0.0117 m` within `0.02 m`).

## Verification Artifacts & Evidence

- **Workspace**: `E:\Stalthosian Archipelago\var\smalltrotter-head-workspace`
- **Seed**: 42, `independent_juvenile`, Blender 5.2.0 LTS
- **Artifacts**:
  - `sources/.../creature.smalltrotter.st1.independent_juvenile.blend`: `SHA256: 0EDE316DE42DE2F9337697977E8FDBB5AEAA82862E19E7057C7BB3A7CE2CC04D`
  - `sources/.../creature.smalltrotter.st1.independent_juvenile.result.json`: `SHA256: D863C25BD56D5D72FA555BB52A9E3DA46C55F7AC60C37B32C80D98633A8EABDB`
  - `preview/.../creature.smalltrotter.st1.independent_juvenile_neutral_090.png`: `SHA256: 10FCB8289AF4CC2DA1A911EC0BD60312E6FCBFA1213AAE6AE000991357A0B77E`
  - `preview/.../creature.smalltrotter.st1.independent_juvenile_neutral_000.png`: `SHA256: EA4FC95AA09FAC1F22118DA452F7CF09AF2E1B10A32E3FA58768FF0A73F42D95`
  - `runtime/.../creature.smalltrotter.st1.independent_juvenile.neutral.glb`: `SHA256: 0F9D93702F0ABF0CC22C1838CD690F192D116104B6AB2E098949E9577186059F`
  - `runtime/.../creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb`: `SHA256: A794283915C79894592586DA9EC2F5C2EAF6C2577545F32C63A0F3A828D553CF`
- **Metrics**:
  - `ventral_face_count`: `578` (covers full ventral throat up to chin)
  - `dark_foot_face_count`: `0`
  - `triangles`: `16028` (budget < 50,000)
  - `neutral_pad_bottom_m`: `0.004` (source pads: `0.004` across all 4 limbs)
  - Rig validation: `passed: true`, `loop_passed: true`, `root_motion_passed: true`, `geometric_contact_passed: true`, `touchdown_foot_lock_passed: true`, `glb_animation_passed: true`
- **Unit Tests**:
  - `tests/test_reference_anatomy.py`: 11 passed in 0.09s
  - `tests/test_reference_surfacing.py`: 4 passed in 0.12s

## Visual Inspection Summary

Inspection of `neutral_090.png` and `neutral_000.png`:
- The segmented ball-and-socket head is replaced by a single, coherent, gently tapered head with a broad, rounded muzzle.
- Eyes are prominent, warm golden-amber lateral eyes with distinct pupils and specular highlights, clearly readable in silhouette and side view.
- Nostrils are subtle, low-profile narial depressions on the dorsal snout.
- Dark patchwork across the face is eliminated; the head presents smooth, warm ochre tones.
- Pale cream throat flows uninterrupted from the chest up the neck and under the jaw.

## Honest Limitations

- Geometry remains an anatomical blockout of interpenetrating shells (body proxy, integument proxy, eyes/nostrils, bristles), not a sculpt-retopologized hero mesh.
- Facial skin wrinkles and scale pores are procedural texture maps rather than displaced micro-geometry.
- Fictional anatomy is speculative based on the single 2D user reference image. No accepted full reference parity is claimed.

## Parent verification — 2026-10-12

Reviewed frozen0f9b833 and corrected hide/eye/nostril UV classification to distance
along the local head tangent instead of world-X thresholds. Fresh parent workspace
`var/smalltrotter-head-verified-asset` build, turntable and rig commands all returned
oktrue/exit0; all18 produced artifact hashes match. Existing authored hind contact,
loop, root-motion and GLB animation checks passed. Root full suite:1988passed,
2skipped in690.71s; changed source/test hashes remained stable.
Actual Wicked run20261012-212633-fab76a21 loads this new GLB, moves1.094296m
and evaluates expected/actualphase1.570139s. The creature still has angular seams,
patchy body hide and provisional feet. This is a head blockout improvement, not
reference parity or visually accepted terrain contact.
