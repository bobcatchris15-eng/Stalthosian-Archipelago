# Smalltrotter Bristle Coherence Refinement

## Implementation Overview
The `smalltrotter_bristle_ridge_blockout` feature now receives coherent rigid bone weights via a dedicated component-level binder `_bind_bristles_to_nearest_bone_by_component`. The existing shared feature binder `_bind_feature_to_nearest_bone` remains unchanged for other features (such as `smalltrotter_eye_and_nostril_blockout`).

Key implementation guarantees:
1. **Linear Connected Traversal:** Each connected fiber mesh component is identified using a bounded linear depth-first search in `bmesh` with explicit `bm.verts.index_update()` and deterministic neighbor sorting. Bmesh resources are safely freed in a `finally` block.
2. **Finite Input Guards:** Robust assertions verify that object and armature matrices are finite, the armature determinant is non-zero and finite, and all vertex coordinates, centroids, projected points, and bone distances are strictly finite.
3. **Deterministic Tie-Breaking:** Eligible deformation bone candidates are sorted lexicographically by name upfront, and nearest-bone selection uses an exact `(distance, bone.name)` key tuple without order-dependent floating-point tolerance windows.
4. **Rigid Binding per Component:** Each fiber component is assigned rigidly to a single bone with weight 1.0 (`REPLACE`), preventing intra-fiber distortion or vertex splitting across bones.

## Measured Evidence

### 1. Dedicated Validation Run
Executed in isolated workspace `var/bristle-coherence-validation-20261013` with seed 42 and stage `independent_juvenile` using Blender 5.2.0 LTS:
- `build_creature` (exit code 0): 1260 vertices, 2184 triangles, 1008 polygons across 84 bristles.
- `rig_creature` (exit code 0):
  - `skinned_feature_geometry.smalltrotter_bristle_ridge_blockout`:
    - `armature_modifier_bound`: true
    - `vertex_group_count`: 75
    - `material_slots_preserved`: 1
    - `vertex_count`: 1260
    - `unassigned_vertex_count`: 0 (pre-binding 1260 warning in `skin()` occurs before feature binding; final unassigned count is 0)
    - `connected_components_bound`: 84
  - `skinned_feature_geometry_passed`: true
  - `source_joint_alignment.max_error_m`: 0.0 (<= 1e-5)
  - All walk-cycle gates preserved: `validation.passed`: true, `loop_passed`: true, `root_motion_passed`: true, `geometric_contact_passed`: true, `touchdown_foot_lock_passed`: true, `glb_animation_passed`: true.
  - All idle gates preserved: `idle.validation.loop_passed`: true, `root_translation_zero`: true.
- `render_turntable` (exit code 0): 4 neutral-pose turntable renders generated (0°, 90°, 180°, 270°).

### 2. Connected Fiber Coherence Verification
Inspecting the generated `creature.smalltrotter.st1.independent_juvenile.rig.blend`:
- Connected components evaluated: 84 / 84
- Coherence rate: **84 / 84 (100.0%)** components strictly bound to a single bone with zero intra-fiber split weighting.
- Bone assignment distribution across 84 bristle fibers:
  - `neck_06`: 11 bristles
  - `neck_07`: 10 bristles
  - `neck_08`: 2 bristles
  - `neck_09`: 2 bristles
  - `neck_10`: 3 bristles
  - `neck_12`: 2 bristles
  - `spine_00`: 1 bristles
  - `spine_02`: 2 bristles
  - `spine_03`: 6 bristles
  - `spine_04`: 8 bristles
  - `spine_05`: 12 bristles
  - `spine_06`: 2 bristles
  - `spine_07`: 10 bristles
  - `tail_03`: 1 bristles
  - `tail_05`: 1 bristles
  - `tail_06`: 2 bristles
  - `tail_08`: 1 bristles
  - `tail_12`: 1 bristles
  - `tail_14`: 2 bristles
  - `tail_15`: 2 bristles
  - `tail_17`: 3 bristles

### 3. Key Artifact Hashes (`var/bristle-coherence-validation-20261013`)
- `sources/.../creature.smalltrotter.st1.independent_juvenile.blend`: `F3A5C93CF8DE374F9D28B5B91B56638B3EE8348819079886CFEE669BDAB1AF74`
- `sources/.../creature.smalltrotter.st1.independent_juvenile.rig.blend`: `4B5DC2346DFC16A7A3A29B88FA7D64034A9F45D3B56DC02ABCDD93876C0CC9F1`
- `runtime/.../creature.smalltrotter.st1.independent_juvenile.neutral.glb`: `7691B6E2658E761ECAD136CC14E511AFBF944B6B3EAFFC5C0179898E86EDBD1F`
- `runtime/.../creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb`: `A05DC160F7B2F74A5FC8FFF8CAE9E701159595364B7070B17D25AFE1DB9EB3E3`
- `preview/.../turntable/creature.smalltrotter.st1.independent_juvenile_neutral_090.png`: `207666F7F0F265D860434658E5FBEC26A25C9E87EE5EC5BEE25454E951A471DA`

### 4. Integration Test
- `tests/test_reference_rig_alignment.py::test_smalltrotter_rig_uses_generated_shape_and_binds_reference_features`: PASSED (1 passed in 29.38s).

## Pending Visual Review
- Neutral turntable renders confirm that bristles form an intact, continuous dorsal fringe without collapsing into the body.
- Visual status remains **pending independent review**: animation and dynamic deformation fidelity under live motion cycles require human visual review in the runtime engine, not automatic acceptance.

## Parent validation
Parentactual NEWvar/bristle-coherence-parent-verified-20261013 build/rig/render exit0,18artifacthashesverified. IndependentBlenderaudit84connectedcomponents,15verticeseach,allrigidoneboneweight1 with0incoherentcomponents. Actualnative131937-aa106f9e walk: foursolevalid, signedtravel0.752164m, phaseactual/expected1.327429896s, captureviewed. Existingwalk/idle/sharedbinder ASTidentical; onlymain dispatchchanged. Rootfullsuite initially4failed/1969passed/15errors dueinstalledoptionaltools enteringmissing-tool fixtures; afterexplicitfixtureisolation fullsuitepassed (rawlogsretained),51optimizer/publish focusedpass. Frozenrig/test/definitionhashesmatch. Noartistic/contact/FPS/fullDraftacceptance.
