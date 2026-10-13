# Rendered Ground Height Parity Diagnostic

## Executive Summary

Prior to this diagnostic, sole clearances were measured purely against `stalthos::domain::ReferenceLandscape`, an analytical procedural heightfield. In Base `34039ba`, idle analytical clearances reported minima of:
- Front Left (FL): -0.041890 m (-42 mm)
- Front Right (FR): -0.040290 m (-40 mm)
- Rear Left (RL): +0.055523 m (+56 mm)
- Rear Right (RR): -0.026497 m (-26 mm)

However, analytical height does not match the actual displayed ground geometry. The engine constructs a 97x97 vertex mesh on a 2.0-meter grid with quad triangulation `{a, b, c, b, d, c}` named `"draft reference ground"`. Between grid vertices, the rendered mesh consists of piecewise flat triangular facets, which deviate from the nonlinear continuous mathematical landscape by up to tens of millimeters.

This diagnostic implements `stalthos::domain::RenderedGroundSurface`, which caches the actual ground mesh vertices and indices transformed by the rendered object instance matrix after `Scene::Update()`. It validates grid topology and spacing in $O(1)$ setup, and samples exact triangle barycentric elevation at the exact world $(x, z)$ coordinates of each selected sole vertex without bilinear smoothing, analytic reconstruction, or per-frame linear mesh scanning.

## Mathematical Formulation

For each quad cell $(i_x, i_z)$ with origin $(x_0, z_0)$ and dimensions $(\Delta x, \Delta z)$:
- Vertex $a = (x_0, z_0)$, height $y_a$
- Vertex $b = (x_0 + \Delta x, z_0)$, height $y_b$
- Vertex $c = (x_0, z_0 + \Delta z)$, height $y_c$
- Vertex $d = (x_0 + \Delta x, z_0 + \Delta z)$, height $y_d$
- Cell diagonal connects $b$ and $c$, forming two triangles:
  - Triangle 0: $\{a, b, c\}$
  - Triangle 1: $\{b, d, c\}$

Normalized local coordinates:
$$u = \frac{x - x_0}{\Delta x}, \quad v = \frac{z - z_0}{\Delta z}$$

### Triangle 0 ($u + v \le 1.0$)
Barycentric weights:
$$w_a = 1.0 - u - v, \quad w_b = u, \quad w_c = v$$
$$y = w_a y_a + w_b y_b + w_c y_c$$

### Triangle 1 ($u + v > 1.0$)
Barycentric weights:
$$w_b = 1.0 - v, \quad w_d = u + v - 1.0, \quad w_c = 1.0 - u$$
$$y = w_b y_b + w_d y_d + w_c y_c$$

On the diagonal $u + v = 1.0$, both formulations evaluate to $u y_b + v y_c$, guaranteeing $C^0$ continuity across triangle boundaries.

## Diagnostic Evidence

### Idle Probe (`duration=3s`, `x=12.0`, `z=0.0`, neutral idle)

| Foot | Vertices | Analytical Min | Rendered Min | Height Diff Range [Min, Max] | Parity Finding |
| :--- | :---: | :---: | :---: | :---: | :--- |
| Front Left | 572 | -0.041890 m (-42 mm) | **+0.036251 m (+36 mm)** | [-0.096006 m, +0.012774 m] | Analytical showed -42 mm penetration; rendered shows **+36 mm hover**. Rendered facet is ~78 mm lower than analytical curve. |
| Front Right | 572 | -0.040290 m (-40 mm) | **-0.040290 m (-40 mm)** | [-0.000000 m, -0.000000 m] | Exact parity ($<0.001$ mm diff) due to proximity to grid line $x=12, z=0$. |
| Rear Left | 588 | +0.055523 m (+56 mm) | **+0.027946 m (+28 mm)** | [+0.024807 m, +0.050271 m] | Rendered facet is ~28 mm higher than analytical curve; true clearance is **+28 mm**, not +56 mm. |
| Rear Right | 588 | -0.026497 m (-26 mm) | **-0.045606 m (-46 mm)** | [+0.017483 m, +0.019741 m] | Rendered facet is ~19 mm higher than analytical curve; penetration is **-46 mm**, not -26 mm. |

Validation flags:
- `strict_complete`: `true` (all 4 feet sample full vertex sets)
- `strict_disjoint`: `true` (sole vertex selectors are non-overlapping)
- `strict_finite`: `true` (all coordinates, heights, and clearances are finite)
- `foot_contact_accepted`: `false` (kinematic surface diagnostic only)

### Walk Probe (`duration=3s`, `--diagnostic-walk`)

| Foot | Vertices | Analytical Min | Rendered Min | Height Diff Range [Min, Max] |
| :--- | :---: | :---: | :---: | :---: |
| Front Left | 572 | -0.036641 m (-37 mm) | **+0.037868 m (+38 mm)** | [-0.095943 m, +0.012804 m] |
| Front Right | 572 | +0.021812 m (+22 mm) | **+0.021812 m (+22 mm)** | [-0.000000 m, -0.000000 m] |
| Rear Left | 588 | +0.084909 m (+85 mm) | **+0.077191 m (+77 mm)** | [+0.006309 m, +0.051661 m] |
| Rear Right | 588 | -0.045553 m (-46 mm) | **-0.045553 m (-46 mm)** | [-0.000000 m, +0.020690 m] |

## Parity Conclusion

1. **Surface of Authority**: The **rendered ground surface (`RenderedGroundSurface`)** is the visible ground seen by the camera and player. Pose alignment and foot clearance should be validated against the rendered triangles rather than the analytical continuum.
2. **Discrepancy Scale**: Across the Smalltrotter's footprint on 2-meter terrain quads, facet deviation reaches up to **96 mm**. Attempting to correct foot placement using analytical height would cause visible foot-hovering or ground clipping of up to ~8-10 cm.
3. **Next Steps**: Now that exact dual-surface parity is measured and cached without per-frame terrain scanning, body root offset and per-foot contact adjustments can be applied knowing the exact distance to the visible ground triangles.

Parent review adds an affine-transform requirement and cache capacity bounds (262,144 vertices / 1,572,864 indices), checked before allocation or dereference. Worker receipts are diagnostic evidence; parent native validation is separate. No pose correction or contact acceptance is included in this change.
