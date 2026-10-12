"""Definition -> bpy params normalization (Python side, before Blender starts).

Worker A owns ``species/`` and the JSON Schemas; this module therefore reads
the definition *tolerantly*: it looks for a key under a list of plausible paths
and falls back to a documented default. It never validates against a schema and
never fails on a missing optional field -- only on a missing/blank definition.

The output is the params payload handed to Blender as JSON. Every stochastic
decision inside Blender is driven by seeds derived here via ``ctx.seed_for``,
so Blender never picks its own.
"""

from __future__ import annotations

from typing import Any, Iterable, Sequence

from ..core.contract import Context

#: Search order for a scalar, most specific first.
def _dig(definition: dict, key: str) -> Any:
    """Depth-first search for the first occurrence of ``key`` in a nested mapping."""
    stack: list[Any] = [definition]
    while stack:
        node = stack.pop(0)
        if isinstance(node, dict):
            if key in node and node[key] is not None:
                return node[key]
            stack.extend(v for v in node.values() if isinstance(v, (dict, list)))
        elif isinstance(node, list):
            stack.extend(v for v in node if isinstance(v, (dict, list)))
    return None


def pick(definition: dict, key: str, default: Any = None, *aliases: str) -> Any:
    for name in (key, *aliases):
        value = _dig(definition, name)
        if value is not None:
            return value
    return default


def pick_section(definition: dict, *names: str) -> dict:
    """Return the first nested mapping found under any of ``names``."""
    for name in names:
        value = _dig(definition, name)
        if isinstance(value, dict):
            return value
    return {}


def as_float(value: Any, default: float) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


PLANT_FAMILIES = ("tree", "fern", "horsetail", "rosette", "shrub")

FAMILY_ALIASES = {
    "tree": "tree", "conifer": "tree", "broadleaf_tree": "tree", "araucarian": "tree",
    "fern": "fern", "frond": "fern", "cycad": "rosette", "rosette": "rosette",
    "horsetail": "horsetail", "equisetum": "horsetail", "reed": "horsetail",
    "shrub": "shrub", "broadleaf_shrub": "shrub", "bush": "shrub",
}

#: ``generator`` enum -> the family this module actually implements. The roster
#: ships 11 plant generators; five families are implemented here. The second
#: element says whether the mapping is exact or an approximation -- an
#: approximation is warned about by the calling command so a reader is never
#: told a bracket fungus really is a shrub.
GENERATOR_FAMILY: dict[str, tuple[str, bool]] = {
    "branching_tree": ("tree", True),
    "tree_frond_crown": ("fern", True),
    "layered_shrub": ("shrub", True),
    "arching_shrub": ("shrub", True),
    "woody_rosette": ("rosette", True),
    "broadleaf_rosette": ("rosette", True),
    "jointed_rush": ("horsetail", True),
    "compound_frond": ("fern", True),
    "fibrous_monocot": ("horsetail", False),
    "bracket_fungus": ("shrub", False),
    "moss_lichen_crust": ("shrub", False),
}


def generator_of(definition: dict) -> str | None:
    """The definition's declared ``generator`` slug, if it has one."""
    value = pick(definition, "generator", None)
    return str(value).strip().lower() if value else None


def family_is_approximation(definition: dict, family: str | None = None) -> bool:
    """True when the implemented family only approximates the declared generator."""
    gen = generator_of(definition)
    if gen is None or gen not in GENERATOR_FAMILY:
        return True
    return not GENERATOR_FAMILY[gen][1]


def resolve_family(definition: dict, override: str | None = None) -> str:
    gen = generator_of(definition)
    if override is None and gen in GENERATOR_FAMILY:
        return GENERATOR_FAMILY[gen][0]
    raw = (override or gen or pick(definition, "family", None)
           or pick(definition, "morphology_family", None)
           or pick(definition, "morphology_id", None)
           or pick(definition, "body_plan", None) or "shrub")
    key = str(raw).strip().lower()
    for junk in ("-", " ", "/", "_"):
        key = key.replace(junk, "_")
    while "__" in key:
        key = key.replace("__", "_")
    # "Araucarian-like conifer" -> the '-like' qualifier carries no family signal
    key = key.replace("_like", "").replace("_analogue", "").replace("_analog", "")
    key = key.strip("_")
    if key in FAMILY_ALIASES:
        return FAMILY_ALIASES[key]
    for family in PLANT_FAMILIES:
        if family in key:
            return family
    for alias, family in FAMILY_ALIASES.items():
        if len(alias) > 4 and alias in key:
            return family
    return "shrub"


def normalize_plant_params(definition: dict, ctx: Context, *, family: str | None = None,
                           variant_index: int = 0, stage: str | None = None,
                           asset_id: str = "") -> dict[str, Any]:
    """Build the full bpy params payload for a plant generation run."""
    family = resolve_family(definition, family)
    seed = ctx.seed_for("generate_plant", asset_id, family, variant_index, stage or "default")
    return {
        "kind": "plant",
        "asset_id": asset_id,
        "family": family,
        "seed": seed,
        "variant_index": variant_index,
        "stage": stage,
        "generator": "stalthos.plant." + family,
        "plant": plant_shape_params(definition, family, stage=stage),
        "variation": plant_variation_params(definition, family, variant_index=variant_index),
    }


def unit_m(value: Any) -> float | None:
    """Read a schema ``unit`` block (``{value, unit}``) as metres, or a bare number."""
    if isinstance(value, dict):
        number = value.get("value")
        unit = str(value.get("unit", "m")).lower()
        if not isinstance(number, (int, float)):
            return None
        if unit.startswith("cm"):
            return float(number) * 0.01
        if unit.startswith("mm"):
            return float(number) * 0.001
        if unit.startswith("k"):
            return float(number) * 1000.0
        return float(number)
    if isinstance(value, (int, float)):
        return float(value)
    return None


def growth_stage_list(definition: dict) -> list[dict[str, Any]]:
    """Normalized growth stages, accepting both schema shapes.

    The schema (contracts/species-definition.schema.json) declares
    ``growth_stages`` as an **ordered array** whose last entry is the reference
    mature form. Older/hand-written definitions use a mapping keyed by stage
    name. Both are normalized here to ``{id, height_m, proportions, ...}``.
    """
    raw = pick(definition, "growth_stages", None)
    if raw is None:
        raw = pick(definition, "life_stages", None) or pick(definition, "stages", None)
    out: list[dict[str, Any]] = []
    if isinstance(raw, list):
        for index, entry in enumerate(raw):
            if not isinstance(entry, dict):
                continue
            stage_id = str(entry.get("id") or entry.get("age_label") or f"stage_{index}")
            out.append({
                "id": stage_id,
                "age_label": entry.get("age_label") or stage_id,
                "height_m": unit_m(entry.get("height")),
                "crown_spread_m": unit_m(entry.get("crown_spread")),
                "proportions": dict(entry.get("proportions") or {}),
                "foliage_density": entry.get("foliage_density"),
                "wind_stiffness": entry.get("wind_stiffness"),
                "visible_traits": list(entry.get("visible_traits") or []),
            })
    elif isinstance(raw, dict):
        for stage_id, block in raw.items():
            if not isinstance(block, dict):
                continue
            # legacy mapping form: keep the original keys visible so absolute
            # scale keys (height_scale, trunk_scale, ...) still resolve
            out.append({
                **block,
                "id": str(stage_id),
                "age_label": str(block.get("age_label") or stage_id),
                "height_m": unit_m(block.get("height_m", block.get("height"))),
                "proportions": dict(block.get("proportions") or {}),
                "raw": block,
            })
    return out


def mature_height_m(definition: dict) -> float | None:
    """Height of the reference mature form (the last declared stage)."""
    stages = growth_stage_list(definition)
    if stages and stages[-1].get("height_m"):
        return float(stages[-1]["height_m"])
    return None


def plant_shape_params(definition: dict, family: str, *, stage: str | None = None) -> dict[str, Any]:
    """Family-specific numeric shape parameters with documented defaults."""
    stage_block = next((s for s in growth_stage_list(definition) if s["id"] == stage), None) or {}

    def num(key: str, default: float, *aliases: str) -> float:
        if key in stage_block and stage_block[key] is not None:
            return as_float(stage_block[key], default)
        return as_float(pick(definition, key, default, *aliases), default)

    mature = mature_height_m(definition)
    height = as_float(pick(definition, "height_m", None, "mature_height_m", "canopy_height_m"),
                      mature if mature else 1.0)
    height = max(0.05, height)
    trunk_d = max(0.002, num("trunk_diameter_m", height * 0.03, "trunk_diameter", "trunk_radius_m"))
    # trunk_diameter_m may legitimately be given as a radius field name upstream
    if "trunk_radius_m" in definition and "trunk_diameter_m" not in definition:
        trunk_d = max(0.004, 2.0 * as_float(definition["trunk_radius_m"], trunk_d * 0.5))

    common = {
        "height_m": height,
        "trunk_diameter_m": trunk_d,
        "leaf_length_m": max(0.01, num("leaf_length_m", height * 0.09, "leaf_length", "frond_length_m")),
        "leaf_width_m": max(0.004, num("leaf_width_m", height * 0.03, "leaf_width", "pinna_width_m")),
        "leaf_segments": int(clamp(num("leaf_segments", 5.0), 2, 16)),
        "twist_deg": clamp(num("twist_deg", 6.0, "leaf_twist_deg"), -60.0, 60.0),
        "tube_sides": int(clamp(num("tube_sides", 8.0), 4, 24)),
    }

    if family == "tree":
        return {
            **common,
            "branch_levels": int(clamp(num("branch_levels", 3.0), 1, 7)),
            "branch_angle_deg": clamp(num("branch_angle_deg", 34.0, "branch_angle"), 5.0, 85.0),
            "divergence_deg": clamp(num("divergence_deg", 137.5, "phyllotaxy_divergence_deg", "phyllotaxy"), 20.0, 360.0),
            "taper": clamp(num("taper", 0.62, "taper_ratio"), 0.25, 0.95),
            "children_per_node": int(clamp(num("children_per_node", 2.0), 1, 5)),
            "node_start": clamp(num("node_start", 0.35, "crown_start"), 0.05, 0.95),
            "node_end": clamp(num("node_end", 0.97, "crown_end"), 0.1, 1.0),
            "nodes_per_level": int(clamp(num("nodes_per_level", 5.0), 2, 14)),
            "leaf_cluster": int(clamp(num("leaf_cluster", 14.0), 0, 60)),
            "root_flare": clamp(num("root_flare", 1.7, "root_flare_ratio"), 1.0, 4.0),
            "leaf_shape": str(pick(definition, "leaf_shape", "elliptic", "leaf_outline")),
        }
    if family == "fern":
        params = {
            **common,
            "frond_count": int(clamp(num("frond_count", 7.0, "fronds", "crowns"), 1, 40)),
            "pinnae_per_side": int(clamp(num("pinnae_per_side", 11.0, "pinnae", "pinna_count"), 4, 40)),
            "pinna_span": clamp(num("pinna_span", 0.62, "pinna_coverage"), 0.2, 0.98),
            "rachis_curve": clamp(num("rachis_curve", 0.45, "frond_curve"), 0.0, 1.4),
            "pinna_taper": clamp(num("pinna_taper", 0.7, "pinna_length_ratio"), 0.2, 0.98),
            "crozier": clamp(num("crozier", 0.35, "fiddlehead"), 0.0, 1.0),
            "narrowness": clamp(num("narrowness", 0.32, "pinna_width_ratio"), 0.05, 1.0),
        }
        # This generator is an architectural fern profile, not the generic
        # ground-level compound_frond form. Keep these additions scoped to its
        # explicit generator so other fern definitions retain their behavior.
        if str(pick(definition, "generator", "")) == "tree_frond_crown":
            anatomy = definition.get("anatomy", {})
            foliage = anatomy.get("foliage", {}) if isinstance(anatomy, dict) else {}
            pinnation = foliage.get("pinnation", {}) if isinstance(foliage, dict) else {}
            stem = anatomy.get("stem", {}) if isinstance(anatomy, dict) else {}
            length = unit_m(foliage.get("length")) if isinstance(foliage, dict) else None
            diameter = unit_m(stem.get("diameter_at_base")) if isinstance(stem, dict) else None
            params.update({
                "crown_trunk_height_m": height * 0.45,
                "frond_length_m": length or height * 0.82,
                "pinna_length_ratio": clamp(as_float(pinnation.get("pinna_length_ratio"), 0.42), 0.2, 0.8),
                "trunk_diameter_m": max(trunk_d, diameter or trunk_d),
                "frond_count": int(clamp(num("frond_count", 9.0, "crowns"), 5, 16)),
                "pinnae_per_side": int(clamp(num("pinnae_per_side", 34.0, "pinna_count"), 8, 40)),
                "pinna_span": 0.98,
                "rachis_curve": 0.78,
                "pinna_taper": 0.78,
                "narrowness": 0.20,
                "crozier": 0.0,
            })
        return params
    if family == "horsetail":
        return {
            **common,
            "stem_count": int(clamp(num("stem_count", 9.0, "stems", "culms"), 1, 60)),
            "segments": int(clamp(num("segments", 9.0, "segment_count", "internodes"), 3, 40)),
            "sheath_ratio": clamp(num("sheath_ratio", 1.22, "node_swelling"), 1.0, 2.2),
            "whorl_branches": int(clamp(num("whorl_branches", 10.0, "whorl_count", "branchlets"), 0, 40)),
            "whorl_length_ratio": clamp(num("whorl_length_ratio", 0.18), 0.02, 0.7),
            "lean_deg": clamp(num("lean_deg", 7.0, "stem_lean_deg"), 0.0, 40.0),
            "sterile_top": clamp(num("sterile_top", 0.18, "sterile_tip_ratio"), 0.0, 0.6),
        }
    if family == "rosette":
        return {
            **common,
            "leaf_count": int(clamp(num("leaf_count", 16.0, "leaves", "rosette_leaves"), 3, 80)),
            "leaf_layers": int(clamp(num("leaf_layers", 3.0), 1, 8)),
            "spread_deg": clamp(num("spread_deg", 46.0, "crown_spread_deg"), 5.0, 88.0),
            "inner_spread_deg": clamp(num("inner_spread_deg", 12.0), 0.0, 60.0),
            "length_jitter": clamp(num("length_jitter", 0.22), 0.0, 0.6),
            "leaf_thickness_m": max(0.0, num("leaf_thickness_m", 0.006)),
            "petiole_ratio": clamp(num("petiole_ratio", 0.35), 0.0, 0.8),
            "stiffness": clamp(num("stiffness", 0.18, "leaf_stiffness"), 0.0, 1.0),
        }
    # shrub (default)
    return {
        **common,
        "stem_count": int(clamp(num("stem_count", 6.0, "stems", "shoots"), 1, 40)),
        "height_ratio": clamp(num("height_ratio", 1.0), 0.3, 2.0),
        "branch_angle_deg": clamp(num("branch_angle_deg", 42.0, "branch_angle"), 8.0, 88.0),
        "branch_levels": int(clamp(num("branch_levels", 2.0), 1, 5)),
        "nodes_per_level": int(clamp(num("nodes_per_level", 4.0), 2, 12)),
        "divergence_deg": clamp(num("divergence_deg", 137.5, "phyllotaxy"), 20.0, 360.0),
        "taper": clamp(num("taper", 0.66, "taper_ratio"), 0.25, 0.95),
        "leaf_per_node": int(clamp(num("leaf_per_node", 2.0, "leaves_per_node"), 0, 8)),
        "arching": clamp(num("arching", 0.5, "arch"), 0.0, 1.5),
        "twig_radius_m": max(0.001, num("twig_radius_m", 0.012)),
    }


#: schema ``variation.seed_labels`` -> the multiplier key this module drives.
VARIATION_LABEL_MAP = {
    "trunk_height": "height",
    "height": "height",
    "stature": "height",
    "leaf_size": "leaf_scale",
    "leaf_length": "leaf_scale",
    "foliage_size": "leaf_scale",
    "branch_angle": "branch_angle",
    "crown_spread": "branch_angle",
}

#: multiplier key -> default multiplicative range when the definition is silent.
DEFAULT_VARIATION_RANGE = {"height": (-0.18, 0.20),
                           "leaf_scale": (-0.12, 0.12),
                           "branch_angle": (-0.12, 0.12)}


def plant_variation_params(definition: dict, family: str, *, variant_index: int) -> dict[str, Any]:
    """Bounded per-variant multipliers. A family must read as one lineage, so the
    ranges come from the definition's own ``variation`` block.

    Two shapes are accepted: the schema's ``variation.ranges`` (a map of
    ``seed_label -> [min, max]``, where the values are *multiplicative* factors
    around 1.0) and the older flat ``variation.<key>_min/_max`` form.
    """
    variation = pick_section(definition, "variation", "variation_ranges", "morphological_variation")
    out: dict[str, Any] = {}
    ranges = variation.get("ranges") if isinstance(variation.get("ranges"), dict) else {}
    for label, pair in ranges.items():
        key = VARIATION_LABEL_MAP.get(str(label).lower())
        if key is None or not isinstance(pair, (list, tuple)) or len(pair) < 2:
            continue
        # schema values are multipliers (1.0 == nominal); store as deltas
        out[key] = (float(pair[0]) - 1.0, float(pair[1]) - 1.0)
    for key, default in DEFAULT_VARIATION_RANGE.items():
        if key in out:
            continue
        lo = variation.get(f"{key}_min")
        hi = variation.get(f"{key}_max")
        if isinstance(lo, (int, float)) and isinstance(hi, (int, float)):
            out[key] = (float(lo), float(hi))
        else:
            out[key] = default
    out["family"] = family
    out["variant_index"] = variant_index
    out["source"] = "definition.variation.ranges" if ranges else "documented defaults"
    return out


def variant_multipliers(ctx: Context, asset_id: str, variation: dict[str, Any],
                        variant_index: int) -> dict[str, float]:
    """Bounded per-variant multipliers, one derived seed each.

    A family must still read as one lineage, so the multipliers are drawn from
    the definition's own ranges (default +-12%) rather than from an unbounded
    random distribution.
    """
    out: dict[str, float] = {}
    for key in ("height", "leaf_scale", "branch_angle"):
        lo, hi = variation.get(key, (-0.12, 0.12))
        out[key] = ctx.rng("variant", asset_id, key, variant_index).uniform(float(lo), float(hi))
    return out


#: shape-parameter key -> which multiplier drives it
MULTIPLIER_TARGETS = {
    "height": ("height_m",),
    "leaf_scale": ("leaf_length_m", "leaf_width_m"),
    "branch_angle": ("branch_angle_deg", "spread_deg"),
}


def apply_scales(shape: dict[str, Any], multipliers: dict[str, float],
                 stage_params: dict[str, Any]) -> dict[str, Any]:
    """Fold variant multipliers and stage-dependent proportions into a shape block.

    Stage scales are applied *after* the variant multipliers so a growth stage is
    always the same shape family regardless of which variant index asked for it.
    """
    out = dict(shape)
    for multiplier, keys in MULTIPLIER_TARGETS.items():
        factor = 1.0 + float(multipliers.get(multiplier, 0.0))
        for key in keys:
            if key in out:
                out[key] = out[key] * factor
    stage_scales = {
        "height_m": stage_params.get("height_scale", 1.0),
        "trunk_diameter_m": stage_params.get("trunk_scale", 1.0),
        "leaf_length_m": stage_params.get("leaf_scale", 1.0),
        "leaf_width_m": stage_params.get("leaf_scale", 1.0),
    }
    for key, factor in stage_scales.items():
        if key in out:
            out[key] = out[key] * float(factor)
    for key in ("frond_count", "stem_count", "leaf_count", "pinnae_per_side",
                "children_per_node", "leaf_per_node", "nodes_per_level", "whorl_branches"):
        if key in out:
            scaled = out[key] * float(stage_params.get("branch_density", 1.0))
            out[key] = max(1, int(round(scaled)))
    if "node_start" in out:
        out["node_start"] = float(stage_params.get("crown_start", out["node_start"]))
    out["variant"] = {
        "multipliers": {k: round(v, 6) for k, v in multipliers.items()},
        "stage": stage_params.get("stage"),
        "stage_scales": {k: v for k, v in stage_params.items() if k.endswith("_scale")},
        "branch_density": stage_params.get("branch_density"),
        "crown_start": stage_params.get("crown_start"),
    }
    return out


def growth_stage_params(definition: dict, *, stage: str | None) -> dict[str, Any]:
    """Stage-dependent proportions, with the neutral 'adult' default."""
    stages = growth_stage_list(definition)
    block = next((s for s in stages if s["id"] == stage), None)
    available = block is not None
    block = block or {}
    props = block.get("proportions") or {}

    def prop(*names: str, default: float = 1.0) -> float:
        for name in names:
            if isinstance(props.get(name), (int, float)):
                return float(props[name])
        return default

    # height_scale is the stage's own height against the mature reference form
    mature = stages[-1].get("height_m") if stages else None
    stage_height = block.get("height_m")
    if isinstance(stage_height, (int, float)) and isinstance(mature, (int, float)) and mature:
        height_scale = clamp(float(stage_height) / float(mature), 0.02, 1.0)
    else:
        height_scale = as_float(block.get("height_scale", prop("height")), 1.0)
    crown_share = prop("crown_share", default=0.0)
    return {
        "stage": stage or (stages[-1]["id"] if stages else "adult"),
        "available": available or stage is None,
        "height_scale": clamp(height_scale, 0.02, 1.0),
        # non-uniform development: the schema's `proportions` block encodes a
        # normalized length share per organ, which is what drives trunk/crown
        "trunk_scale": clamp(as_float(block.get("trunk_scale", prop("stem_share", default=1.0)), 1.0), 0.05, 1.0),
        "leaf_scale": clamp(as_float(block.get("leaf_scale", 1.0), 1.0), 0.05, 1.0),
        "branch_density": clamp(as_float(block.get("branch_density") or block.get("foliage_density")
                                            or crown_share or 1.0, 1.0), 0.05, 1.0),
        "crown_start": clamp(as_float(block.get("crown_start", 0.35), 0.35), 0.05, 0.95),
        "proportions": props,
        "age_label": block.get("age_label"),
        "visible_traits": block.get("visible_traits") or [],
        "extra": {k: v for k, v in block.items()
                  if k not in {"height_m", "proportions", "foliage_density", "id", "age_label"}},
    }


def creature_shape_params(definition: dict, *, stage: str | None = None) -> dict[str, Any]:
    """Proportional body plan for the quadruped anatomical *proxy*.

    These are shape/proportion parameters for a pipeline-verification mesh, not
    paleontological measurements. See the module docstring of the bpy script.

    The creature schema expresses the plan as *shares*: ``body_plan.neck_length_share``
    / ``tail_length_share`` / ``axial.ribcage_depth_share``, per-segment
    ``length_share`` on ``body_plan.skeleton``, and absolute ``total_length`` per
    ``life_stages`` entry. Those shares are resolved against the stage's total
    length here. The legacy absolute-key form is still accepted as a fallback.
    """
    stages = _creature_stage_list(definition)
    block = next((s for s in stages if s["id"] == stage), None) or {}
    if not block:
        # No explicit stage: use the reference mature form. The schema array's
        # last entry is that form; prefer a named mature/adult entry when one
        # exists so a definition that also lists a later non-adult stage (e.g.
        # a senescent form) is not silently treated as the adult.
        for preferred in ("adult", "mature", "mature_form"):
            block = next((s for s in stages if str(s["id"]).lower() in (preferred, f"{preferred}_form")), None) or {}
            if block:
                break

    plan = pick_section(definition, "body_plan", "anatomy", "proportions")
    axial = plan.get("axial") if isinstance(plan.get("axial"), dict) else {}
    skeleton = plan.get("skeleton") if isinstance(plan.get("skeleton"), list) else []
    posture = str(plan.get("posture") or "quadrupedal")

    def absolute(key: str, default: float) -> float:
        """Stage block wins; a legacy absolute key on the plan is the fallback."""
        if key in block:
            return as_float(block.get(key), default)
        if key in plan:
            return as_float(plan.get(key), default)
        return default

    total_length = unit_m(block.get("total_length"))
    if not total_length:
        total_length = as_float(pick(definition, "total_length_m", None, "length_m", "adult_length_m"), 5.0)
    total_length = max(0.4, float(total_length))

    def share_of(segment_id: str) -> float | None:
        for entry in skeleton:
            if isinstance(entry, dict) and str(entry.get("id")) == segment_id:
                value = entry.get("length_share")
                return float(value) if isinstance(value, (int, float)) else None
        return None

    def count(key: str, default: int, *aliases: str) -> int:
        for source in (axial, block):
            for name in (key, *aliases):
                value = source.get(name)
                if isinstance(value, (int, float)):
                    return int(value)
        return default

    neck_share = as_float(plan.get("neck_length_share", block.get("neck_share", 0.30)), 0.30)
    tail_share = as_float(plan.get("tail_length_share", block.get("tail_share", 0.34)), 0.34)
    depth_share = as_float(axial.get("ribcage_depth_share", 0.17), 0.17)
    skull_share = share_of("skull") or 0.055
    girth = as_float(block.get("girth_scale"), 1.0)

    trunk_length = max(0.2, total_length * (1.0 - clamp(neck_share, 0.0, 0.6) - clamp(tail_share, 0.0, 0.7)))
    props = block.get("proportions") if isinstance(block.get("proportions"), dict) else {}

    def prop(*names: str, default: float) -> float:
        for name in names:
            if isinstance(props.get(name), (int, float)):
                return float(props[name])
        return default

    return {
        "total_length_m": total_length,
        "trunk_length_m": absolute("trunk_length_m", trunk_length),
        "trunk_depth_m": absolute("trunk_depth_m", total_length * depth_share),
        "trunk_width_m": absolute("trunk_width_m", total_length * depth_share * 0.72),
        "neck_segments": int(clamp(count("cervical_count", 9, "neck_segments"), 3, 30)),
        "neck_length_m": absolute("neck_length_m", total_length * neck_share),
        "neck_rise_deg": clamp(as_float(block.get("neck_rise_deg"), 34.0), -20.0, 85.0),
        "head_length_m": absolute("head_length_m", total_length * skull_share * as_float(block.get("head_scale"), 1.0)),
        "head_depth_m": absolute("head_depth_m", total_length * depth_share * 0.5),
        "tail_length_m": absolute("tail_length_m", total_length * tail_share),
        "tail_segments": int(clamp(count("caudal_count", 14, "tail_segments"), 4, 40)),
        "tail_rise_deg": clamp(as_float(block.get("tail_rise_deg"), 14.0), -20.0, 60.0),
        "leg_upper_m": absolute("leg_upper_m", total_length * 0.15),
        "leg_lower_m": absolute("leg_lower_m", total_length * 0.13),
        "leg_diameter_m": absolute("leg_diameter_m", total_length * 0.035),
        "leg_spread_deg": clamp(as_float(block.get("leg_spread_deg"), 9.0), 0.0, 40.0),
        "stance_height_m": absolute("stance_height_m", unit_m(block.get("height_at_shoulder")) or total_length * 0.28),
        "trunk_segments": int(clamp(count("dorsal_count", 10, "trunk_segments"), 4, 40)),
        "trunk_arch": clamp(absolute("trunk_arch", 0.05), -0.25, 0.35),
        "tube_sides": int(clamp(as_float(block.get("tube_sides"), 10.0), 6, 24)),
        "posture": posture,
        "girth_scale": clamp(girth, 0.1, 2.0),
        "crown_share": prop("crown_share", default=0.0),
        "stage": stage or (block.get("id") if block else "adult"),
        "proxy_note": "anatomical proxy for pipeline verification; not fossil measurements",
    }


def _creature_stage_list(definition: dict) -> list[dict[str, Any]]:
    """Normalized life stages from the schema's ordered array (legacy map too)."""
    raw = pick(definition, "life_stages", None)
    if raw is None:
        raw = pick(definition, "growth_stages", None) or pick(definition, "stages", None)
    out: list[dict[str, Any]] = []
    if isinstance(raw, list):
        for index, entry in enumerate(raw):
            if not isinstance(entry, dict):
                continue
            stage_id = str(entry.get("id") or entry.get("label") or f"stage_{index}")
            out.append({**entry, "id": stage_id})
    elif isinstance(raw, dict):
        for stage_id, entry in raw.items():
            if isinstance(entry, dict):
                out.append({**entry, "id": str(stage_id)})
    return out


def creature_stage_names(definition: dict) -> list[str]:
    return [s["id"] for s in _creature_stage_list(definition)] or ["adult"]


def creature_surface_params(definition: dict) -> dict[str, Any]:
    """Integument density, read from the schema's zone maps.

    The schema expresses integument as ``scale_zones`` / ``osteoderm_zones``
    entries with a ``size_mm`` range and ``density_per_m2``. The proxy collapses
    those to a coarse density so the bpy script can place surface detail; the
    per-zone placement itself is an approximation and is warned about by
    ``build_creature``.
    """
    surface = pick_section(definition, "integument", "surface", "covering", "skin")
    zones = surface.get("scale_zones") if isinstance(surface.get("scale_zones"), list) else []
    osteo = surface.get("osteoderm_zones") if isinstance(surface.get("osteoderm_zones"), list) else []
    bristles = surface.get("bristle_zones") if isinstance(surface.get("bristle_zones"), list) else []
    densities = [z.get("density_per_m2") for z in zones if isinstance(z, dict)
                 and isinstance(z.get("density_per_m2"), (int, float))]
    mean_density = sum(densities) / len(densities) if densities else None
    return {
        "osteoderm_rows": int(clamp(len(osteo), 0, 40)),
        "osteoderm_density": clamp(as_float(pick(surface, "osteoderm_density", 0.0), 0.0), 0.0, 1.0),
        "bristle_density": clamp(as_float(pick(surface, "bristle_density", 0.0), 0.0), 0.0, 1.0),
        "bristle_length_m": as_float(pick(surface, "bristle_length_m", 0.02), 0.02),
        "scale_size_m": as_float(pick(surface, "scale_size_m", 0.02), 0.02),
        "skin_wear": clamp(as_float(pick(surface, "wear", 0.3), 0.3), 0.0, 1.0),
        "scale_zone_count": len(zones),
        "osteoderm_zone_count": len(osteo),
        "bristle_zone_count": len(bristles),
        "mean_scale_density_per_m2": mean_density,
        "source": "definition.integument" if zones or osteo else "documented defaults",
    }


def render_settings(definition: dict, ctx: Context, overrides: dict[str, Any] | None = None) -> dict[str, Any]:
    preset = str(pick(definition, "lighting_preset", "neutral", "light_preset") or "neutral")
    settings = {
        "preset": preset,
        "resolution": int(clamp(as_float(pick(definition, "preview_resolution", 512.0), 512.0), 64, 4096)),
        "samples": int(clamp(as_float(pick(definition, "preview_samples", 32.0), 32.0), 1, 4096)),
        "engine": str(pick(definition, "engine", "EEVEE") or "EEVEE"),
        "seed": ctx.seed_for("render", preset),
        "turntable_angles": [0, 90, 180, 270],
        "transparent": False,
        "film_exposure": 0.0,
        "view_transform": "Standard",
    }
    for key, value in (overrides or {}).items():
        if value is not None:
            settings[key] = value
    return settings


def iter_definitions(paths: Iterable[str]) -> list[str]:  # pragma: no cover - helper
    return [str(p) for p in paths if str(p)]


def as_list(value: Any) -> list[Any]:  # pragma: no cover - helper
    if value is None:
        return []
    if isinstance(value, (list, tuple)):
        return list(value)
    return [value]


def stage_names(definition: dict) -> list[str]:
    names = [s["id"] for s in growth_stage_list(definition)]
    return names or ["adult"]


def variant_names(definition: dict, count: int) -> list[str]:
    variation = pick_section(definition, "variation")
    variants = variation.get("variants") or pick(definition, "variants", None)
    if isinstance(variants, list) and variants:
        names = [str(v.get("name") or v.get("id") or f"variant_{i:02d}")
                 if isinstance(v, dict) else str(v) for i, v in enumerate(variants)]
        return names[:count] or ["variant_00"]
    return [f"variant_{i:02d}" for i in range(max(1, count))]


def bounded(value: float, lo: float, hi: float) -> float:
    return clamp(value, lo, hi)


def sequence(values: Sequence[float]) -> list[float]:
    return [float(v) for v in values]
