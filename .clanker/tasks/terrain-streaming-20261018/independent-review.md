# Independent terrain streaming review

Reviewed 2026-10-18 (UTC): domain/terrain_tiles.hpp, tests/terrain_tiles.cpp, engine/terrain_stream_probe.hpp, main.cpp/Run.ps1/CMake changes, and vendor Wicked Engine creation/removal implementation. HALT absent before review. Implementation was not edited; no native application launched.

Decision: no integration blocker found in the reviewed bounded optional construction probe. Native capture and integrated runtime validation remain coordinator-owned.

Evidence and checks:
- Ran var/build/stalthos-runtime/Release/Stalthos_terrain_tiles.exe: {"status":"success","suite":"terrain_tiles","groups":3}.
- Shared edges use global signed integer lattice coordinates before float conversion. Sampled elevation, finite-difference normals and habitat values are deterministic for a fixed seed/configuration; both adjacency axes and negative coordinates are exercised. Source boundary queries clamp finite-difference endpoints safely. Configuration/world/key/radius guards bound allocations and prevent integer overflow.
- Tile mesh winding matches Wicked Engine convention. Although ordinary cross(b-a,c-a) faces -Y, vendor cube top faces also have negative cross with positive authored normals. The supplied source-field normals face +Y and are assigned explicitly after CreateMeshFromData.
- Vendor Entity_CreateCube positions span local Y=-1..1. Y scale .5 and center translation ground+.5 place its bottom center at sampled ground. This supports a bottom-center residual only; it does not establish full bottom-face contact on slopes. Coordinator was notified and is renaming the metric proxy_bottom_center_error_m.
- Entity_CreateMeshFromData places its ObjectComponent, MeshComponent and MaterialComponent on the same entity, with meshID/materialID referencing it. Entity_Remove removes that entity from every component manager. No orphan material ownership was found for streamed tiles.
- Probe maintains the requested resident neighborhood, removes only its own entities, rebuilds entrants synchronously and queries the same float vertex positions/topology sent to the renderer. Interactive traversal stays well inside complete source tiles. Return receipt explicitly states missing watershed/population/water integration and synchronous-streaming limitations.
- CLI prevents combination with populated world, diagnostic modes, persistence and asset arguments. CMake adds the terrain suite under the existing compile/test conventions.

Nonblocking follow-ups / scope limits:
- The automated path crosses several tiles in one direction; it does not demonstrate eviction then return/reconstruction in the native host. Domain reconstruction and order checks cover exact source reconstruction, but native reverse travel should be included before claiming traversal retention robustness.
- The source-field normal seam check establishes equal supplied normals, not equal geometric face normals or verified final GPU shading. Captures remain necessary.
- Very short user-selected durations may complete with fewer crossings or captures. Receipt counters should be evaluated by the coordinator; mere successful process exit is not acceptance.
- The optional probe does not change the populated watershed's actual terrain/activation. It is a construction checkpoint, not completion of integrated world streaming.

## Wrapper follow-up review (2026-10-18)

Read-only review of Verify-TerrainStreaming.ps1 and bounded receipt mutation tests under var/reports/terrain-review-negative/. No engine edit or native launch.

- Corrected native receipt var/reports/runtime/20261018-024054-de63bf4c passed wrapper checks.
- Stale receipt var/reports/runtime/20261018-023907-ea823515 rejected because max_proxy_bottom_center_error_m is missing.
- Missing, null and NaN bottom-center metrics each rejected with the explicit invalid-metric error. Source evidence and three images were copied to var before mutation.
- Two hardening findings sent to coordinator: finite negative bottom-center error (-1) passed the upper-bound-only threshold; string captures ["true","true","true"] passed PowerShell coerced comparisons. Recommend nonnegative numeric guard for all these counts/errors and strict boolean/length validation for captures. These are fail-closed receipt-verification gaps; they do not invalidate the positive native receipt measured above.
- Wrapper checks native mode, five boundary crossings, six visited tiles, nine peak residents, fifteen evictions, positive shared-edge/support counts, exact shared-edge height/normal equality, bottom-center error threshold, capture flags and image existence. Image existence is not image-content or visual acceptance. Latest native visual acceptance remains coordinator-owned and pending after proxy visibility investigation.

Coordinator fixes reverified: negative fixture now rejected by nonnegative numeric guard; string-captures fixture now rejected by strict three-boolean capture validation. Corrected positive native receipt still passes. Both wrapper findings are resolved; no remaining wrapper blocker identified. Native visual acceptance is still separate.

## Proxy visibility follow-up

Independently opened the exact native files var/reports/runtime/20261018-024650-23f6faaa/terrain-0.png, terrain-1.png and terrain-2.png using the image viewer. All three visibly contain the red proxy, approximately centered at pixel (792,626), with apparent size about 46 by 38 pixels. Files have distinct hashes, lengths and terrain horizon differences. The reported late missing-marker symptom was not reproduced in this candidate. No additional native launch performed.

Read-only renderer check: RenderPath3D defaults its scene and camera pointers to GetScene()/GetCamera(). Update copies the camera into the scene and calls Scene::Update; PreRender culls through the current camera. CameraComponent::TransformCamera updates Eye/At/Up and View as expected; ClearTransform marks the component dirty. No concrete stale-camera or tile-eviction cause was established. The most immediate next check is candidate/image identity when comparing any allegedly missing-marker capture. This visual confirmation covers the proxy construction probe only, not final art, water joins, full world streaming or performance acceptance.
