# Scenery ground contact calibration

`environment_scene.hpp` now determines each prop's vertical placement after its heading, pitch, and roll are applied. It uses the same quaternion multiplication sequence as Wicked Engine's `TransformComponent::RotateRollPitchYaw`, rotates every generated mesh vertex, and samples `ReferenceLandscape::height` at each resulting world X/Z. The prop is raised by the smallest amount that leaves all in-bounds vertices at or above those analytic samples. This replaces the former local-Y-only offset that allowed tilted logs and stones to float.

The reference landscape is an analytic function while the runtime terrain is rendered as a finite triangulated mesh. Vertex contact is exact against the analytic reference samples and approximate against the rendered triangles between samples. The calculation checks mesh vertices, not every point along triangle interiors; this is a vertex-fit placement helper, not a terrain collision solver. Root integration should recapture and inspect the rendered placements.

For scale, the generated log has an unscaled length of 0.7–1.45 m and base diameter of about 0.16–0.24 m. Proposal scale remains authored by `HabitatDressing` (0.8–2.2), so typical results read as 1–3 m branches against a 1.8 m juvenile, with occasional larger wood. Pebble dimensions and prop budgets are unchanged. These are visual calibration choices, not ecological measurements.

The focused receipt records an actual Wicked-header Release compile, transform comparison, and numeric analytic vertex-fit results. It does not claim visual acceptance; integrated capture review remains necessary.
