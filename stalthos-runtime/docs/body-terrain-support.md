# Body Terrain Support Integration

`stalthos-runtime/domain/body_terrain_support.hpp` provides a pure C++17, engine-agnostic domain helper for computing a visual-orientation body coordinate frame adapted to terrain geometry. The Wicked engine host (`stalthos-runtime/engine/main.cpp`) integrates this helper to pitch and roll the creature according to local terrain slopes.

## Scope and Principles

- **Proposed Visual Alignment Only**: Body plane alignment is an approximate visual orientation adjustment; it is **not** foot-lock acceptance, nor does it claim or implement per-foot inverse kinematics (IK) or contact resolution.
- **No Gameplay/Physics Drift**: Analytical positioning, velocity, gait cycle phase, and stride lengths remain completely unaffected.
- **Decoupled Math**: Domain calculations use plain-old-data (POD) structures and standard C++17 mathematics with no DirectX or Wicked Engine dependencies.

## Coordinate Basis and Transform Composition

The terrain support helper and Wicked host maintain a consistent **Left-Handed (LH)** coordinate convention:
- Forward = `+Z`
- Up = `+Y`
- Right = `+X`

### Basis Construction
1. Flat forward `(sin(yaw), 0, cos(yaw))` and right `(cos(yaw), 0, -sin(yaw))` are established.
2. Terrain heights are sampled at analytical center `(x, z)` and four cardinal offsets at `longitudinalHalfspan` (0.5 m) and `lateralHalfspan` (0.25 m).
3. Finite differences establish slope vectors along forward and right directions.
4. Up normal is derived via LH cross product: `rawUp = cross(vecF, vecR)`. Note `rawUp.y = 1.0` by construction.
5. Up normal is normalized with overflow-safe scaling.
6. Flat forward is projected onto the tangent plane defined by the Up normal:
   $$\vec{f}_{\text{proj}} = \vec{f}_{\text{flat}} - (\vec{f}_{\text{flat}} \cdot \hat{u}) \hat{u}$$
7. Right vector is orthogonalized: $\vec{r} = \hat{u} \times \hat{f}$.

### DirectX Row-Vector Composition
In DirectX / Wicked Engine row-vector conventions:
- The support basis matrix $R_{\text{support}}$ has rows: Row 0 = Right, Row 1 = Up, Row 2 = Forward.
- The authored creature asset has its longitudinal axis aligned to $+X$.
- The asset-to-host offset is $R_{\text{offset}} = \text{XMMatrixRotationY}(-\pi / 2)$.
- The composite transform is formed in row-vector order:
  $$M = R_{\text{offset}} \cdot R_{\text{support}}$$
- When terrain support is flat or unsupported, this evaluates identically to $R = \text{XMMatrixRotationY}(\text{yaw} - \pi / 2)$.

## Slope Threshold and Fallback

- Slopes exceeding 20° ($\approx 0.349066$ radians) are rejected as unsupported (`supported = false`).
- When unsupported, the helper explicitly falls back to the flat yaw basis (`up = (0, 1, 0)`), while recording the true surface slope and normal.
- The Wicked host falls back to flat yaw orientation and increments the `supportfailure` receipt counter.

## Finite Arithmetic Safeguards

- **Input Validation**: All coordinates `(x, z, yaw)` and sample halfspans must be finite; halfspans must be strictly positive.
- **Callback Safety**: Non-finite return values or exceptions from height callbacks are trapped and reported as typed exceptions.
- **Overflow Prevention**: Normalization scales components by their maximum absolute value, preventing square-sum overflow.
- **Domain Clamping**: `std::clamp(up.y, -1.0, 1.0)` guards `std::acos` from generating `NaN` due to floating point precision artifacts.

## A/B Testing and Diagnostic Probes

- **Opt-out Flag**: `--disable-terrain-support` forces flat yaw orientation for comparative evaluation.
- **Receipt Fields**:
  - `supportmode` / `support_mode`: `"terrain_supported"`, `"unsupported_yaw_fallback"`, or `"disabled"`.
  - `slope` / `support_slope_radians`: current slope angle in radians.
  - `normal` / `support_normal`: sampled surface normal vector `{x, y, z}`.
  - `supportfailure` / `support_failure_count`: cumulative count of unsupported or failed support frames.

## Parent matched runtime evidence — 2026-10-12

Reviewed8ee38f3 and additionally reject overflow of doubled sample halfspans;
new regression case compiled and passed. Root Release build and17/17 native
CTest pass. Proxy and creature orientation are approximate body-plane support,
with explicit unsupported fallback and no per-foot IK claim.

Matched frozen-walk-phase-zero captures use the same pose(12,0), zero body
movement and phase0. Disabled:20261012-233900-f534b8fa. Enabled:
20261012-233927-5d2900ad. Both runs report slope0.214139rad (about12.27degrees)
and sampled normal(0.211232,0.977160,-0.023235); enabled applies the frame with
zero fallback failures. Body tilt changes visibly. These are not neutral-idle or
foot-contact acceptance: the pose is still a paused walk frame and individual
feet may remain above or below terrain. --DisableTerrainSupport enables repeatable
comparison through Run.ps1.
