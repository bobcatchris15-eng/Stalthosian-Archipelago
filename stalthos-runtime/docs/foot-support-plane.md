# Foot Support Plane Helper

The `foot_support_plane.hpp` module provides a deterministic, offline-capable routine to fit a local coordinate basis and least-squares plane to exactly four foot positions. This tool is a foundation for diagnostic terrain alignment, procedural posture tilting, and locomotion clearance estimation.

## Centered, Rescaled 2x2 Least Squares Formulation

Initial prototypes used an uncentered 3x3 normal equation system solved via Cramer's rule:

$$
\begin{bmatrix}
\sum x_i^2 & \sum x_i z_i & \sum x_i \\
\sum x_i z_i & \sum z_i^2 & \sum z_i \\
\sum x_i & \sum z_i & 4
\end{bmatrix}
\begin{bmatrix}
A \\ B \\ C
\end{bmatrix}
=
\begin{bmatrix}
\sum x_i y_i \\ \sum z_i y_i \\ \sum y_i
\end{bmatrix}
$$

When foot samples are evaluated at realistic island world coordinates (e.g. $x \sim 10^3$ to $10^6$ m), uncentered sums like $\sum x_i^2$ grow quadratically ($10^{12}$), and the resulting 3x3 matrix columns become nearly linearly dependent. This causes severe ill-conditioning, catastrophic cancellation when subtracting large matrix products, and spurious arithmetic overflow.

To resolve this, the module computes a **centered and rescaled 2x2 least-squares system**:

1. **Centroid Normalization**:
   We evaluate the centroid $(\bar{x}, \bar{y}, \bar{z})$:
   $$\bar{x} = \frac{1}{4}\sum x_i, \quad \bar{y} = \frac{1}{4}\sum y_i, \quad \bar{z} = \frac{1}{4}\sum z_i$$
   Centering offsets $dx_i = x_i - \bar{x}$, $dy_i = y_i - \bar{y}$, $dz_i = z_i - \bar{z}$ ensures $\sum dx_i = 0$ and $\sum dz_i = 0$. The constant term $C$ decouples completely to $\bar{y}$, reducing the system to $dy = A \cdot dx + B \cdot dz$.

2. **Horizontal Span Rescaling**:
   We determine the maximum horizontal offset $S = \max_i (\max(|dx_i|, |dz_i|))$.
   Normalized horizontal coordinates $u_i = dx_i / S$ and $w_i = dz_i / S$ are strictly bounded within $[-1, 1]$.
   Solving for rescaled slopes $A' = A \cdot S$ and $B' = B \cdot S$:
   $$
   \begin{bmatrix}
   \sum u_i^2 & \sum u_i w_i \\
   \sum u_i w_i & \sum w_i^2
   \end{bmatrix}
   \begin{bmatrix}
   A' \\ B'
   \end{bmatrix}
   =
   \begin{bmatrix}
   \sum u_i dy_i \\ \sum w_i dy_i
   \end{bmatrix}
   $$
   The normalized determinant $det' = M_{uu} M_{ww} - M_{uw}^2$ is bounded in $[0, 16]$. This rescaling prevents arithmetic underflow on microscopic footprints ($10^{-15}$ m) and prevents overflow on large coordinates.

3. **Translation Invariance**:
   - The fitted slopes $A = A' / S$ and $B = B' / S$, the upward normal vector $\vec{N} = \text{normalize}(-A, 1, -B)$, and the slope angle $\theta = \arccos(\vec{N}_y)$ are strictly invariant under arbitrary translations $(T_x, T_y, T_z)$.
   - Center height is directly $\bar{y}$, translating exactly by $T_y$ with zero cancellation error.
   - Residuals are evaluated directly in centered space:
     $$r_i = dy_i - (A' u_i + B' w_i)$$
     avoiding catastrophic subtraction of large world coordinates.

## Input Validation and Distinct Points Policy

The solver requires exactly four finite, distinct foot samples:

- **Finiteness**: Any non-finite sample coordinate ($NaN, \pm\infty$) or non-finite yaw throws `std::invalid_argument`. Coordinates with magnitudes triggering quadratic overflow ($> 10^{150}$) throw `std::runtime_error`.
- **Distinct Footprints**: All four points must be spatially distinct. If any pair of points shares identical 3D coordinates $(x, y, z)$ or identical horizontal footprints $(x, z)$, or if relative normalized distance $(u_i - u_j)^2 + (w_i - w_j)^2 < 10^{-14}$, the function throws `std::invalid_argument`.
- **Collinearity Rejection**: If all four samples lie along a line in the horizontal plane ($det' \le 10^{-12}$), the layout cannot define a 2D support plane and throws `std::invalid_argument`.

## 20-Degree Maximum Tilt Policy and Fallback Basis

Similar to the overarching domain orientation system (`body_terrain_support.hpp`), the helper evaluates the terrain normal derived from the fitted support plane.

If the plane deviates more than 20 degrees from horizontal (i.e. $\theta > \text{kMaxSlopeAngleRadians}$), or if the normal does not face upward ($\vec{N}_y \le 0$):
- The configuration is explicitly marked unsupported: `supported = false`.
- The local orthonormal basis (right, up, forward) falls back to a perfectly flat frame based on the root yaw:
  $$\vec{up} = (0, 1, 0), \quad \vec{forward} = (\sin(\text{yaw}), 0, \cos(\text{yaw})), \quad \vec{right} = (\cos(\text{yaw}), 0, -\sin(\text{yaw}))$$
- Rather than inventing artificial success on unnavigable geometry, the routine bounds unsupported states explicitly.

When the slope is within 20 degrees, an orthonormal basis retains the requested horizontal bearing by solving the vertical forward component against the plane normal, then deriving the right axis by a cross product.

## Raw Residuals and Separation from Feet IK

The helper returns four vertical residuals $r_i$, one for each input foot sample, relative to the actual fitted plane equation:

- Crucially, residuals are computed against the **actual fitted plane** even when the slope exceeds 20 degrees and triggers the flat-basis fallback.
- This fulfills the architectural directive to avoid silently masking foot hovering or penetration with arbitrary offsets.
- **Scope Boundary**: This helper is strictly an offline mathematical plane estimator. It does not perform inverse kinematics (IK), sole deformation, or limb posing. Downstream engine and animation controllers consume the unbiased residuals and support frame to drive gait adjustments and contact verification.

Parent review removes claims of arbitrary-coordinate precision and adds an actual diagonal-slope horizontal-heading regression. This helper is not yet used to alter the native body pose or feet.
