# Native sole surface diagnostic

The adapter caches four disjoint toe-dominant sole vertex groups from body_proxy, with a rest-Y band of0..48mm and a4096-vertex cap per foot. It excludes the separate keratin mesh and tail/calves. All dereferenced joint lanes, finite nonnegative normalized weights, supported influence count and current boneData capacity are validated. Empty, overlapping or partially invalid sample groups are unresolved.

Parent review follows the pinned engine picking path at wiScene.cpp8890..8896: SkinVertex(mesh, armature, index), then scene.matrix_objects[scene.objects.GetIndex(bodyObject)]. This matches the renderer object instance transform, which may differ from armature world. The worker's armature-world conversion and resulting negative clearances are rejected. No custom matrix memcpy or silent fallback to the first armature is used.

Reports give actual selected body-mesh surface minimum/maximum/mean vertical terrain clearance and world endpoints. These remain kinematic diagnostics; no foot contact, IK or visual acceptance is claimed. Selection is initialized once; per-frame reports still allocate strings/vectors, so zero-allocation and performance claims are not made. Parent native idle/walk evidence must confirm the mapping before further pose corrections.
