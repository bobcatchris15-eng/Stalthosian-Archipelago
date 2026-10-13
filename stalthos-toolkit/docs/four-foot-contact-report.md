# Four-Foot Contact Diagnostics

Starting with this revision, `rig_creature` reports authored contact validation for the configured hind targets and, when serialized front pad data is available, separate front-limb kinematic diagnostics. These scopes remain distinct.

## Rationale
The authored `contacts` definition specifies target rules only for hind limbs, so only those targets participate in contact validation. Front pad measurements are diagnostics. Their nominal stance windows are inferred from the existing kinematic gait phase sequence; they do not establish an authored front contact schedule or acceptance.

## Diagnostic Measurements
For `manus_hand_l` and `manus_hand_r` (mapped to `toe_front_L` and `toe_front_R` in the proxy rig), `rig.py` reports:
- **`foot_pad_vertex_counts`**: The exact number of vertices bound to the front pad proxy meshes.
- **`coverage`**: Per-selector availability and vertex count. Missing, empty, or invalid serialized indices are reported as unavailable with a reason; they are not silently treated as zero samples.
- **`ground_geometry`**: Sample counts and minimum measured gaps across the full animation loop.
- **`nominal_stance_samples`**: Samples collected when the inferred kinematic phase indicates a planted status.
- **`touchdown_anchored_stance_excursion`**: The maximum horizontal drift from touchdown placement, taking root-motion extraction into account.
- **`stance_samples`**: Raw per-frame bounding metrics (z minima and center coordinates) for every stance frame.

## Unaccepted Status
These front-limb metrics are grouped under `diagnostic_unaccepted_front_limbs` inside the walk-cycle `contacts` report and labeled `kinematic diagnostic only`. They do not participate in authored checks or validation. When serialized front pad data is missing or malformed, per-selector coverage records the unavailable reason and no front metrics are claimed. Legacy sources without serialized joint data omit front diagnostics because their front pad coverage is unknown.

## Parent Blender and native evidence — 2026-10-12

Collected Jules18391627216232906812 in isolation; reviewed correction8bcbbae
makes coverage explicit and preserves authored hind validation. Fresh rig command
in `var/smalltrotter-head-verified-asset` samples522vertices and49frames per
front foot. Inferred stance clearance spans2.425–11.066mm; maximum full-loop
clearance116.493mm includes swing. Measured maximum horizontal touchdown
excursion is1.811mm. Zero-tolerance summary rows are diagnostic summaries, not
front-foot acceptance criteria. Existing hind gates remain unchanged and passed.

Root suite1988passed/2skipped; actual asset artifacts match their status hashes.
Native run20261012-212633-fab76a21 imports and animates the new GLB. Its creek-bank
support changes108.1mm across0.5m, while these authoring measurements use flat
z=0. Native per-foot terrain support and explicit authored front schedules remain
unaccepted; low flat-ground error must not be reported as terrain-foot-lock proof.
