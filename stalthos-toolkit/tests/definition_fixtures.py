"""Schema-valid definition fixtures for tests/test_blender.py.

The YAML lives in one place and is validated against contracts/*.schema.json
exactly like a checked-in asset.

Every fixture is written under tmp_path, so a run never writes into the source
tree. `PLANT_SCHEMA_FIXTURES`/`CREATURE_*` intentionally exercise the *current*
definition shape: `growth_stages` as an ordered array whose last entry is the
mature reference form, and `variation.ranges` keyed by seed label.
"""

from __future__ import annotations

import textwrap
from pathlib import Path

PLANT_TEMPLATE = """
schema: species-definition
kind: plant
id: {id}
common_names:
  primary: Testspec
  working: true
uncertainty: speculative
morphology_family: Open woodland broadleaf (analogy only)
generator: {generator}
anatomy:
  branching_pattern:
    architecture: decurrent_multi_leader
    order_max: {order_max}
  phyllotaxy:
    pattern: alternate
    divergence_deg: 137.5
  foliage:
    organ: leaf
    overall_shape: ovate
    margin: entire
    length: {{value: {leaf_length}, unit: m}}
    width: {{value: {leaf_width}, unit: m}}
  stem:
    type: woody
    diameter_at_base: {{value: {trunk_d}, unit: m}}
  root:
    system: taproot
growth_stages:
- id: seedling
  age_label: seedling
  height: {{value: {seedling_h}, unit: m}}
  proportions:
    stem_share: 1.0
    crown_share: 0.05
- id: mature
  age_label: mature
  height: {{value: {height_m}, unit: m}}
  proportions:
    stem_share: 0.4
    crown_share: 0.6
materials:
- id: bark
  applies_to: bark
  surface_type: bark
affordances:
  forage_tags:
  - {{part: leaf, palatability: 0.4, water_content: 0.6}}
  contact_bounds:
    min_browse_height_m: 0.2
    max_browse_height_m: {height_m}
variation:
  seed_labels: [trunk_height, leaf_size, branch_angle]
  ranges:
    trunk_height: [0.85, 1.15]
    leaf_size: [0.9, 1.1]
    branch_angle: [0.88, 1.12]
habitat_tags: [open_woodland]
references:
- id: plan
  kind: planning_doc
  locator: Rauken-Archipelago/docs/flora.md
  usage_rights: internal_reference
licenses:
- id: defn
  applies_to: definition
  license: CC0-1.0
"""

CREATURE_TEMPLATE = """
schema: creature-definition
kind: creature
id: {id}
common_names:
  primary: Testquad
  working: true
uncertainty: speculative
morphology_family: low-slung quadrupedal form (analogy only)
generator: low_slung_quadruped
evidence:
- feature: quadrupedal proxy body plan
  tier: speculative
  note: invented for pipeline verification; no fossil claim is asserted
body_plan:
  posture: {posture}
  spine_profile: horizontal
  neck_length_share: 0.16
  tail_length_share: 0.36
  mass_estimate:
    value: 320
    unit: kg
    measured: false
  axial:
    cervical_count: 7
    dorsal_count: 12
    caudal_count: 26
    ribcage_depth_share: 0.17
    skull_profile: broad_flanged
  skeleton:
  - id: skull
    region: skull
    length_share: 0.055
  - id: cervical
    region: cervical
    length_share: 0.16
  - id: dorsal
    region: dorsal
    length_share: 0.48
  - id: caudal
    region: caudal
    length_share: 0.36
life_stages:
- id: juvenile
  label: Juvenile
  age_label: juvenile
  total_length: {{value: 1.2, unit: m}}
  height_at_shoulder: {{value: 0.4, unit: m}}
  proportions: {{skull: 0.08, neck: 0.18, trunk: 0.5, tail: 0.24}}
  girth_scale: 0.7
- id: adult
  label: Adult
  age_label: adult
  total_length: {{value: 3.8, unit: m}}
  height_at_shoulder: {{value: 1.2, unit: m}}
  proportions: {{skull: 0.05, neck: 0.16, trunk: 0.48, tail: 0.36}}
  girth_scale: 1.0
rig:
  skeleton:
    bone_count_range: [40, 90]
    hierarchical: true
    root: root
    bone_naming: region_index
  ik:
  - id: front_leg
    type: leg
    chain: [front_L]
  skin:
    method: automatic_bones
animation_contacts:
- id: browse
  kind: browse
  targets:
  - selector: bone:jaw_tip
    surface: plant
integument:
  scale_zones:
  - id: zone_dorsal
    region: dorsal
    pattern: rosette
    size_mm: [8, 24]
    density_per_m2: 1400
  osteoderm_zones:
  - id: zone_upper_back
    region: upper_back
    shape: keeled
    count_range: [40, 70]
    size_mm: [10, 30]
material_masks:
- id: hide
  applies_to: body
  surface_type: hide
affordances:
  diet_tags:
  - resource: tender_foliage
    preference: 0.6
  - resource: small_prey
    preference: 0.4
  interaction_envelope:
    reach_height_m: {{value: 1.4, unit: m}}
    reach_radius_m: 1.0
    mouth_tip_selector: bone:jaw_tip
habitat_tags: [riparian]
references:
- id: plan
  kind: planning_doc
  locator: Rauken-Archipelago/docs/creatures.md
  usage_rights: internal_reference
licenses:
- id: defn
  applies_to: definition
  license: CC0-1.0
"""


def plant_yaml(asset_id: str = "plant.test.sp1", *, generator: str = "branching_tree",
               height_m: float = 2.4, trunk_d: float = 0.06,
               leaf_length: float = 0.09, leaf_width: float = 0.03,
               order_max: int = 2) -> str:
    """A minimal definition that satisfies contracts/species-definition.schema.json."""
    return PLANT_TEMPLATE.format(id=asset_id, generator=generator, height_m=height_m,
                                 seedling_h=round(float(height_m) / 4.0, 4),
                                 trunk_d=trunk_d, leaf_length=leaf_length,
                                 leaf_width=leaf_width, order_max=order_max).strip() + "\n"


def creature_yaml(asset_id: str = "creature.test.cr1", *, posture: str = "quadrupedal") -> str:
    """A minimal definition that satisfies contracts/creature-definition.schema.json."""
    return CREATURE_TEMPLATE.format(id=asset_id, posture=posture).strip() + "\n"


def write(path: Path, text: str) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(text), encoding="utf-8")
    return path