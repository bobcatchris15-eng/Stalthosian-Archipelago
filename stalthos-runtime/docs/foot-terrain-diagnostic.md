# Per-Foot Terrain Clearance Diagnostic Design & Implementation

`stalthos-runtime/engine/foot_terrain_diagnostic.hpp` provides a pure C++17, engine-decoupled diagnostic helper for sampling individual four-foot ground clearances against the authoritative reference landscape (`stalthos::domain::ReferenceLandscape`).

---

## 1. Context & Motivation

In previous integration revisions, creature orientation on terrain was handled by `calculateTerrainSupport` (`stalthos-runtime/domain/body_terrain_support.hpp`). That helper samples five points around the creature center at longitudinal (0.5 m) and lateral (0.25 m) halfspans to construct an orthonormal body basis matrix, and tilts the entire root transform (`heroMotionRoot`).

While body plane alignment visually tilts the creature on slopes, it does **not** evaluate individual foot clearance:
- The four feet move independently in world space during locomotion (or in neutral stance).
- Natural terrain features local slope variation, micro-topography, creek banks, and ridges where a flat body plane approximation causes individual feet to float significantly above or penetrate deep below the surface.
- Authored walk-cycle contact validation in the asset pipeline was performed on flat $z = 0$ ground; native runtime terrain contact has remained explicitly unaccepted.

This helper establishes the mathematical interface, data structures, and diagnostic reporting needed to measure per-foot clearance directly against the authoritative landscape.

---

## 2. Four Foot Identity Mapping & Rig Landmarks

The diagnostic helper maps all four limbs to canonical identifiers, concrete Blender rig export bone names, and toolkit selectors:

| Identity | Canonical ID String | Blender Rig Bone (`wi::ecs::Entity`) | Toolkit Selector | Anatomical Role |
| :--- | :--- | :--- | :--- | :--- |
| **Front Left** | `"front_left"` | `toe_front_L` | `diagnostic:manus_hand_l` | Manus (Front) |
| **Front Right** | `"front_right"` | `toe_front_R` | `diagnostic:manus_hand_r` | Manus (Front) |
| **Rear Left** | `"rear_left"` | `toe_rear_L` | `bone:pes_foot_l` | Pes (Hind) |
| **Rear Right** | `"rear_right"` | `toe_rear_R` | `bone:pes_foot_r` | Pes (Hind) |

Hind limbs participate in authored contact rules; front limbs are tracked as kinematic diagnostics only.

### Identity Validation & Mapping Guarantees
Inputs provided via `evaluateFourFeetFromBoneProxies` or `evaluateFourFeetFromVertexClouds` are validated to ensure all 4 distinct foot identities (`FrontLeft`, `FrontRight`, `RearLeft`, `RearRight`) are present exactly once:
- **Duplicate Detection**: Passing duplicate foot IDs (e.g., two `FrontLeft` inputs) is flagged with an explicit typed issue (`"duplicate foot identity in bone proxy inputs"` or `"duplicate foot identity in vertex clouds"`) and `allValid = false`.
- **Missing Identities**: If any canonical foot ID is missing, the report marks `allValid = false` with issue `"missing foot identity..."`.
- **Invalid Enums**: Any out-of-range foot enum value is flagged with `"invalid foot identity"`.
- **Permutation Invariance**: Disordered inputs (e.g. `{RearRight, FrontLeft, RearLeft, FrontRight}`) are mapped deterministically to their matching canonical slots in `report.feet[]`.

---

## 3. Skinned Sole-Pad Surface vs Bone Proxy Distinction

A core requirement is distinguishing between actual sole-pad surface contact and rigid bone-proxy landmarks:

### A. Sole-Pad Surface (`MeasurementMethod::SolePadSurface`)
- **Definition**: The physical bottom contact surface of the skinned foot mesh, evaluated from a bounded cloud of skinned world-space vertex positions (`FootPadVertexCloud`).
- **Bounded Capacity**: Each foot pad cloud is strictly capped at 4096 vertices (`kMaxVerticesPerFootPad = 4096`). Exceeding this cap immediately marks the foot invalid with typed error `"vertex cloud exceeds maximum capacity of 4096 vertices per foot"`.
- **All-Vertex Validity Requirement**: If **any** vertex in the pad cloud is non-finite, out of bounds, or suffers arithmetic overflow, the entire cloud is flagged invalid (`"vertex cloud contains invalid vertex: ..."`) and `allValid = false`. Partial validity is never accepted as overall validity.
- **Clearance Resolution**: For a fully valid cloud, the helper resolves the minimum clearance:
  $$\text{clearance} = \min_{v \in \text{PadVertices}} \left( v_y - H(v_x, v_z) \right)$$

### B. Bone Proxy (`MeasurementMethod::BoneProxy`)
- **Definition**: The world position of the toe bone joint landmark (`toe_front_L`, `toe_front_R`, `toe_rear_L`, `toe_rear_R`), adjusted downward along the world vertical axis by an authored pad thickness offset:
  $$p_{\text{sole}} = \left( p_{\text{joint}, x}, \, p_{\text{joint}, y} - h_{\text{offset}}, \, p_{\text{joint}, z} \right)$$
- **Offset Validation**: Pad offset $h_{\text{offset}}$ must be finite and non-negative ($h_{\text{offset}} \ge 0.0$). Negative or non-finite offsets are rejected with typed error `"negative or non-finite vertical pad offset"`.
- **Explicit Limitations**: Bone proxy queries reflect rigid joint landmarks, not deformable sole-pad skin geometry. The diagnostic report enforces `report.footContactAccepted = false`. Clearance measurements never imply foot acceptance.

---

## 4. Coordinate Transformations & Clearance Arithmetic

The coordinate conventions follow Left-Handed / DirectX standards consistent with Wicked Engine:
- **World Basis**: $+X = \text{Right}$, $+Y = \text{Up}$, $+Z = \text{Forward}$.
- **Blender Rig Alignment**: Authored creature asset longitudinal axis is $+X$. The host applies a $-\pi / 2$ yaw rotation ($R_{\text{offset}}$) to map asset $+X$ to host $+Z$.
- **Signed Clearance Definition**:
  $$\text{clearance} = y_{\text{contact}} - H(x_{\text{contact}}, z_{\text{contact}})$$

### Purely Geometric Thresholds (Not Authored Gait Phases)
The flags `inStanceContact`, `inSwing`, and `penetrating` are **solely geometric elevation distance threshold metrics** relative to `contactToleranceMeters`:
- **Geometric Stance Contact**: $|\text{clearance}| \le \text{tolerance}$
- **Geometric Swing (Elevated)**: $\text{clearance} > \text{tolerance}$
- **Geometric Penetration**: $\text{clearance} < -\text{tolerance}$, with $\text{penetrationDepth} = -\text{clearance}$

> [!IMPORTANT]
> These flags do **not** represent authored walk-cycle gait phases, stance schedules, or dynamic foot locking. They are instantaneous geometric elevation clearances only.

### Input Validation, Bounds Safety & Overflow Hardening
1. **Clearance Subtraction Overflow**: If $y_{\text{contact}} - H$ overflows double-precision finite range (e.g. subtracting huge oppositely signed values), the sample is marked invalid (`valid = false`, `issue = "clearance subtraction non-finite or overflow"`).
2. **Tolerance Validation**: Contact tolerance must be finite and non-negative. Negative or non-finite tolerances are rejected (`issue = "contact tolerance must be finite and non-negative"`).
3. **Measurement Method Validation**: `MeasurementMethod::Unresolved` or out-of-range enums are rejected (`issue = "unresolved or invalid measurement method"`).
4. **Patch Boundary Checks**: Queries outside the finite 192 m patch ($|x| > 96\text{ m}$ or $|z| > 96\text{ m}$) are rejected safely without crashing.
5. **Incremental Aggregate Mean**: `FourFootDiagnosticReport::meanClearanceMeters` uses an incremental (Welford) recurrence `mean += (c - mean) / count`, preventing finite-sum overflow when summing large clearances across feet.
6. **Include Cleanliness**: Headers use straightforward relative include `#include "../domain/reference_landscape.hpp"` without spurious `__has_include` or `#ifdef || true` guards.

---

## 5. Integration Wiring in `main.cpp`

The following snippet illustrates wiring this helper in `stalthos-runtime/engine/main.cpp`:

```cpp
#include "engine/foot_terrain_diagnostic.hpp"

if (creatureLoaded) {
    std::array<stalthos::engine::FootBoneProxyInput, 4> footInputs;
    bool allBonesFound = true;
    for (size_t i = 0; i < stalthos::engine::kFootCount; ++i) {
        const auto& spec = stalthos::engine::kWickedFootLandmarks[i];
        wi::ecs::Entity boneEntity = scene.Entity_FindByName(spec.boneName);
        if (boneEntity != wi::ecs::INVALID_ENTITY) {
            const auto* transform = scene.transforms.GetComponent(boneEntity);
            if (transform) {
                DirectX::XMFLOAT3 pos = transform->GetPosition();
                footInputs[i] = {
                    spec.foot,
                    {pos.x, pos.y, pos.z},
                    spec.defaultSoleThicknessMeters
                };
            } else allBonesFound = false;
        } else allBonesFound = false;
    }

    if (allBonesFound) {
        auto footReport = stalthos::engine::evaluateFourFeetOnLandscapeFromBoneProxies(
            footInputs, 0.02, landscape);

        // Telemetry export:
        // telemetry["foot_diagnostics"] = {
        //     {"method", stalthos::engine::measurementMethodToString(footReport.overallMethod)},
        //     {"all_valid", footReport.allValid},
        //     {"any_penetrating", footReport.anyPenetrating},
        //     {"max_penetration_m", footReport.maxPenetrationMeters},
        //     {"min_clearance_m", footReport.minClearanceMeters},
        //     {"max_clearance_m", footReport.maxClearanceMeters},
        //     {"mean_clearance_m", footReport.meanClearanceMeters},
        //     {"foot_contact_accepted", footReport.footContactAccepted} // strictly false
        // };
    }
}
```

---

## 6. Offline Test Coverage & Verification

Tests are located in `stalthos-runtime/tests/foot_terrain_diagnostic.cpp`. All tests use Release-active validation checks (`check()` throwing `std::runtime_error`) with zero dependencies on Wicked Engine, DirectX, or the network:

1. **`testFootIdentityAndMappings`**: 4 canonical IDs, bone names (`toe_front_L/R`, `toe_rear_L/R`), selectors, front/hind predicates, and string parsing.
2. **`testMatrix4x4Transform`**: Identity, translation, yaw rotation, matrix multiplication, and Left-Handed point transformation.
3. **`testFlatTerrainClearance`**: Geometric contact, elevated clearance, and penetration depth on a flat plane.
4. **`testSlopedTerrainClearance`**: Clearance computation on an inclined plane ($H(x, z) = 0.1x + 0.05z + 3.0$), verifying aligned vs unaligned placement.
5. **`testAuthoritativeReferenceLandscape`**: Deterministic evaluation against `stalthos::domain::ReferenceLandscape(1)` at known landscape coordinates.
6. **`testOutOfBoundsAndInvalidCoordinates`**: Rejection of coordinates outside 192 m patch, non-finite values (`NaN`, $\pm\infty$), and exception-throwing callbacks.
7. **`testSolePadSurfaceVsBoneProxyDistinction`**: Method enums, limitations reporting, and enforcement of `footContactAccepted = false`.
8. **`testWickedLandmarkAndTransformHelper`**: Constant landmark specifications and creature world point computation with $-\pi / 2$ rotation offset.
9. **`testRegressionParentReviewHardening`**: Release-active regression coverage for:
   - Clearance subtraction overflow detection (`valid = false`, issue string).
   - Rejection of negative / non-finite contact tolerances.
   - Rejection of `Unresolved` and invalid measurement method enums.
   - Duplicate, missing, and permuted foot identity validation in bone inputs and vertex clouds.
   - Rejection of negative and non-finite vertical pad offsets.
   - Rejection of partially invalid vertex clouds.
   - Hard cap of 4096 vertices per foot pad and empty cloud rejection.
   - Avoidance of finite-sum overflow in aggregate mean clearance.
   - Standalone callback overflow handling and large finite coordinate boundary safety.

### Test Compilation & Execution Command

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cd /d "E:\Stalthosian Archipelago\var\worktrees\foot-terrain-diagnostic" && cl /nologo /EHsc /std:c++17 /W4 /WX /permissive- /I"stalthos-runtime" "stalthos-runtime\tests\foot_terrain_diagnostic.cpp" /Fe:"var\tests\foot_terrain_diagnostic.exe" && var\tests\foot_terrain_diagnostic.exe'
```

Output:
```
**********************************************************************
** Visual Studio 2026 Developer Command Prompt v18.6.0
** Copyright (c) 2026 Microsoft Corporation
**********************************************************************
[vcvarsall.bat] Environment initialized for: 'x64'
foot_terrain_diagnostic.cpp
Running foot terrain diagnostic tests...
All 9 foot terrain diagnostic tests (including hardening regressions) PASSED successfully.
```

Optimized Release build verification:
```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cd /d "E:\Stalthosian Archipelago\var\worktrees\foot-terrain-diagnostic" && cl /nologo /O2 /DNDEBUG /EHsc /std:c++17 /W4 /WX /permissive- /I"stalthos-runtime" "stalthos-runtime\tests\foot_terrain_diagnostic.cpp" /Fe:"var\tests\foot_terrain_diagnostic_release.exe" && var\tests\foot_terrain_diagnostic_release.exe'
```

Output:
```
[vcvarsall.bat] Environment initialized for: 'x64'
foot_terrain_diagnostic.cpp
Running foot terrain diagnostic tests...
All 9 foot terrain diagnostic tests (including hardening regressions) PASSED successfully.
```

Parent host integration samples named toe transforms after Wicked updates and emits four separate world position, terrain height and vertical clearance values. The 20mm downward offset is speculative, and no sole surface/contact acceptance is claimed. Parent review adds an opposite-sign safe finite running-mean regression.
