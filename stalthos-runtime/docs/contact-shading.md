# Contact Shading and Sun Shadow Restoration

## Summary

In base `6627994`, creature body support was aligned with the visible rendered terrain surface, achieving measured idle sole penetrations of only -0.90 mm to -1.30 mm. However, visual contact still appeared weak and floaty because the rendered scene lacked directional shadows and ambient occlusion.

This change diagnoses the pinned Wicked Engine v0.72.120 lighting and render pipeline flags, restores missing directional sun shadow casting on the host sun entity, and enables bounded native Multi-Scale Ambient Occlusion (`AO_MSAO`) on `wi::RenderPath3D`. No geometry, albedo, terrain support, root elevation, animation, camera, or save data was modified.

## Engine Flag Diagnosis (Wicked Engine v0.72.120)

### Directional Light (`wi::scene::LightComponent`)
In `stalthos-runtime/engine/main.cpp`, the directional sun was created via:
```cpp
auto sun = scene.Entity_CreateLight("draft sun", {0, 30, 0}, {1, .96f, .9f}, 5, 1000, wi::scene::LightComponent::DIRECTIONAL);
```
In pinned Wicked Engine v0.72.120 (`wiScene_Components.h` line 1355), `LightComponent::_flags` initializes to `EMPTY = 0`. The shadow-casting flag `CAST_SHADOW` (`1 << 0`) is not enabled by default on newly created lights. In `wiRenderer.cpp` (lines 4743, 6960), shadow map generation and shadow atlas packing require:
```cpp
const bool shadowmap = IsShadowsEnabled() && light.IsCastingShadow() && !light.IsStatic();
```
Because `sun` never had `SetCastShadow(true)` called, `light.IsCastingShadow()` evaluated to `false`. Wicked Engine completely bypassed directional shadow map passes, resulting in zero directional shadows across the entire scene.

### Mesh and Material Shadow Flags
Inspection of pinned Wicked Engine defaults confirmed that:
- `wi::scene::ObjectComponent::_flags` defaults to `RENDERABLE | CAST_SHADOW` (`wiScene_Components.h` line 1181). Both imported creature meshes and the procedural terrain object had `CAST_SHADOW` active.
- `wi::scene::MaterialComponent::_flags` defaults to `CAST_SHADOW` (`wiScene_Components.h` line 142), and `DISABLE_RECEIVE_SHADOW` was false.
The geometry and materials were already configured to cast and receive shadows; the absence of ground shadows was entirely due to the unflagged `LightComponent`.

### Ambient Occlusion (`wi::RenderPath3D`)
`main.cpp` explicitly configured:
```cpp
renderPath.setAO(wi::RenderPath3D::AO_DISABLED);
```
Wicked Engine v0.72.120 natively supports `AO_MSAO` (Multi-Scale Ambient Occlusion) implemented through standard compute shaders without requiring hardware raytracing (DXR) or proprietary libraries.

## Implementation

In `stalthos-runtime/engine/main.cpp`:
1. Queried `scene.lights.GetComponent(sun)` and called `sunLight->SetCastShadow(true);`.
2. Configured `renderPath.setAO(wi::RenderPath3D::AO_MSAO);`.
3. Kept material albedo, textures, terrain support calculations, and camera configurations unchanged.

## Verification

### Native Test Suite
All 22 native CTest suites pass:
```
100% tests passed, 0 tests failed out of 22
Total Test time (real) = 0.98 sec
```

### Matched Native Captures
Comparing the baseline parent capture against the corrected contact-shading run with identical parameters (`--creature ... --creature-sole-offset 0 --inspection-camera --duration 6`):
- Baseline parent run: `var/reports/runtime/20261013-113032-b0aa4269` (`draft-host.png`)
- Contact-shading run: `var/reports/runtime/20261013-114727-2cda6074` (`draft-host.png`)

#### Measured Idle Sole Clearances
| Foot | Baseline Rendered Minimum | Contact-Shading Rendered Minimum | Status |
| :--- | ---: | ---: | :--- |
| Front Left | -0.90 mm (-0.0008985 m) | -0.90 mm (-0.0008985 m) | Unchanged |
| Front Right | -0.91 mm (-0.0009065 m) | -0.91 mm (-0.0009065 m) | Unchanged |
| Rear Left | -1.30 mm (-0.0012967 m) | -1.30 mm (-0.0012967 m) | Unchanged |
| Rear Right | -1.30 mm (-0.0013005 m) | -1.30 mm (-0.0013005 m) | Unchanged |

All sole selections remained complete, disjoint, and finite with 572/572/588/588 vertices. Zero terrain support fallbacks occurred (`rendered_ground_support_fallbacks: 0`, `support_failure_count: 0`).

#### Visual Grounding Comparison
- **Before (`20261013-113032-b0aa4269`)**: Zero shadow under the creature. Despite geometric contact within ~1 mm of the triangle mesh, the paws appeared detached and floating above the surface.
- **After (`20261013-114727-2cda6074`)**: Oblique directional sunlight projects an accurate shadow of the body, limbs, and neck directly onto the visible terrain facets. Ambient occlusion deepens the contact crevice directly beneath the sole pads, adding visible contact cues while preserving the authored albedo. Whole-cycle contact and shadow accuracy are not accepted.

## Limits

- **Hardware and Performance**: Target hardware is GTX 1080 + Ryzen 1600. Memory and GPU cycle costs of MSAO and shadow mapping remain unmeasured; frame times are not externally metered (`frame_time_measured: false`). No artificial FPS claims are made.
- **Visual Acceptance**: Photoreal appearance is false (`visual_accepted: false`), full draft is incomplete (`full_draft_complete: false`), and foot contact acceptance remains strictly false (`foot_contact_accepted: false`).
- **Kinematics**: No per-foot inverse kinematics, soft-ground substrate displacement, or dynamic locking are implemented.

## Parent review
Parent Release build and22/22CTest passed. Actual matched idle20261013-130539-2ccb5ba1 has visible creature/foliage/terrain shadows, reviewedagainst113032baseline. Fourbodysoleminima exactlymatchbaseline, strictcomplete/disjoint/finite true; no geometry displacement. MSAO andsunshadow costunmeasured. Worker92a9dab frozenoriginal; native sourcechanged only lightflag andAO mode.
