"""Procedural creature material authoring helpers (Blender 5.2 / bpy).

Provides deterministic material construction helpers for fictional creature surfacing,
focusing on the Smalltrotter dwarf titanosaur:
- Flank / dorsal pebbled skin (subtle rosette scales, restrained albedo variation, micro-roughness)
- Ventral / underside skin (lighter, softer, flatter scales, subtle warm undertone)
- Foot / claw keratin (fibrous lengthwise striations, higher specular/lower roughness, denser structure)

Rules:
- Pure parameter helpers and color palettes are importable without bpy for offline tests.
- All randomness is explicit and deterministic from a supplied integer seed via SHA-256 derivation.
- No global random, no hash(), no time-varying inputs (#frame).
- Built via 3D Texture Coordinate -> Object mapping so surface structure exists independently
  of any scene lighting, ready for glTF PBR baking.
"""

from __future__ import annotations

import hashlib
from typing import Any, Dict, Tuple

# Try importing bpy; if bpy is not present (offline/test environment), keep pure helpers available.
try:
    import bpy
    _HAS_BPY = True
except ImportError:
    bpy = None
    _HAS_BPY = False


def deterministic_float(seed: int, label: str, salt: int = 0) -> float:
    """Generate a deterministic float in [0.0, 1.0) using sha256."""
    key = f"{int(seed)}:{label}:{int(salt)}".encode("utf-8")
    digest = hashlib.sha256(key).digest()
    n = int.from_bytes(digest[:8], byteorder="big", signed=False)
    return (n >> 11) / (1 << 53)


def deterministic_range(seed: int, label: str, low: float, high: float, salt: int = 0) -> float:
    """Deterministic float in [low, high]."""
    f = deterministic_float(seed, label, salt)
    return low + f * (high - low)


# Color definitions as RGBA tuples (linear color space)
# Fictional Smalltrotter dorsal: warm ochre/tan skin with dark brown mottling and pebble tones
SMALLTROTTER_DORSAL = {
    "kind": "smalltrotter_dorsal",
    # Warm ochre midtone with darker, clearly separated brown markings.
    "base": (0.36, 0.25, 0.14, 1.0),
    "accent": (0.48, 0.35, 0.21, 1.0),
    "mottling": (0.075, 0.038, 0.018, 1.0),
    "rough": (0.65, 0.45),  # (max, min)
    "bump": 0.40,
    "scale_scales": (1.8, 9.0, 38.0),
    "voronoi_scale": 18.0,
    "subsurface": 0.08,
    "subsurface_radius": (0.015, 0.010, 0.006),
}

# Fictional Smalltrotter underside: pale cream/buff throat, chest and belly, softer folds
SMALLTROTTER_UNDERSIDE = {
    "kind": "smalltrotter_underside",
    "base": (0.62, 0.52, 0.38, 1.0),
    "accent": (0.72, 0.62, 0.47, 1.0),
    "mottling": (0.36, 0.27, 0.18, 1.0),
    "rough": (0.58, 0.38),
    "bump": 0.22,
    "scale_scales": (3.5, 18.0, 80.0),
    "voronoi_scale": 35.0,
    "subsurface": 0.12,
    "subsurface_radius": (0.020, 0.012, 0.008),
}

# Fictional Smalltrotter foot/claw keratin: dense, directional striations, low roughness, dark horn tone
SMALLTROTTER_KERATIN = {
    "kind": "smalltrotter_keratin",
    "base": (0.08, 0.07, 0.06, 1.0),
    "accent": (0.14, 0.12, 0.10, 1.0),
    "mottling": (0.05, 0.05, 0.04, 1.0),
    "rough": (0.42, 0.22),
    "bump": 0.50,
    "scale_scales": (6.0, 32.0, 120.0),
    "wave_scale": 55.0,
    "subsurface": 0.03,
    "subsurface_radius": (0.008, 0.005, 0.003),
}

CREATURE_PRESETS: Dict[str, Dict[str, Any]] = {
    "smalltrotter_dorsal": SMALLTROTTER_DORSAL,
    "smalltrotter_underside": SMALLTROTTER_UNDERSIDE,
    "smalltrotter_keratin": SMALLTROTTER_KERATIN,
}


def get_creature_material_spec(preset_key: str, seed: int = 0) -> Dict[str, Any]:
    """Retrieve material spec with deterministic minor seed variations in albedo and roughness.

    Guarantees no exaggerated noise or wild color swings: restrained variations (< 5%).
    Importable and testable offline without bpy.
    """
    if preset_key not in CREATURE_PRESETS:
        raise ValueError(f"Unknown creature preset: '{preset_key}'. Choose from: {sorted(CREATURE_PRESETS.keys())}")

    base_spec = CREATURE_PRESETS[preset_key]
    spec = dict(base_spec)

    # Restrained seed-derived tint offset (+- 3% brightness/saturation shift)
    f_tone = deterministic_range(seed, f"{preset_key}_tone", 0.97, 1.03, 1)
    b_col = tuple(min(1.0, max(0.0, c * f_tone)) if i < 3 else c for i, c in enumerate(base_spec["base"]))
    a_col = tuple(min(1.0, max(0.0, c * f_tone)) if i < 3 else c for i, c in enumerate(base_spec["accent"]))

    # Restrained roughness drift (+- 0.03)
    r_drift = deterministic_range(seed, f"{preset_key}_rough", -0.025, 0.025, 2)
    rough_max = min(0.95, max(0.2, base_spec["rough"][0] + r_drift))
    rough_min = min(rough_max - 0.05, max(0.1, base_spec["rough"][1] + r_drift))

    spec["base"] = b_col
    spec["accent"] = a_col
    spec["rough"] = (round(rough_max, 4), round(rough_min, 4))
    spec["seed_derived"] = True
    spec["seed"] = seed
    return spec


if _HAS_BPY:
    def _nodes(mat: bpy.types.Material):
        return mat.node_tree, mat.node_tree.nodes, mat.node_tree.links

    def _new_material(name: str) -> bpy.types.Material:
        mat = bpy.data.materials.get(name)
        if mat is not None:
            return mat
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        nt = mat.node_tree
        for node in list(nt.nodes):
            nt.nodes.remove(node)
        return mat

    def _noise(
        nt: bpy.types.NodeTree,
        scale: float,
        location: Tuple[float, float],
        detail: int = 4,
        roughness: float = 0.55,
        distortion: float = 0.0,
        seed_w: float = 0.0,
    ) -> bpy.types.Node:
        node = nt.nodes.new("ShaderNodeTexNoise")
        node.location = location
        node.noise_dimensions = "4D"
        node.inputs["Scale"].default_value = float(scale)
        node.inputs["Detail"].default_value = float(detail)
        node.inputs["Roughness"].default_value = float(roughness)
        if "Distortion" in node.inputs:
            node.inputs["Distortion"].default_value = float(distortion)
        node.inputs["W"].default_value = float(seed_w % 997.0)
        return node

    def build_creature_material(
        name: str,
        preset: str,
        *,
        seed: int = 0,
        scale_hint: float = 1.0,
        subsurface: float | None = None,
        output: str = "principled",
    ) -> bpy.types.Material:
        """Construct a deterministic procedural creature material for Blender 5.2.

        Args:
            name: Material datablock name.
            preset: One of 'smalltrotter_dorsal', 'smalltrotter_underside', 'smalltrotter_keratin'.
            seed: Seed for variation.
            scale_hint: Geometric scale multiplier.
            subsurface: Optional override for subsurface weight.
            output: 'principled' (PBR authored), 'emission' (unlit albedo bake target),
                    'roughness' (unlit roughness bake target), or 'field'.
        """
        if output not in {"principled", "emission", "roughness", "field"}:
            raise ValueError(
                f"Unknown creature material output: {output}. Expected 'principled', 'emission', 'roughness', or 'field'."
            )
        spec = get_creature_material_spec(preset, seed=seed)
        mat = _new_material(name)
        nt, nodes, links = _nodes(mat)
        for node in list(nodes):
            nt.nodes.remove(node)

        out = nodes.new("ShaderNodeOutputMaterial")
        out.location = (850, 0)

        if output in {"emission", "roughness"}:
            surface = nodes.new("ShaderNodeEmission")
            surface.location = (550, 0)
            color_socket, scalar_socket = "Color", "Strength"
            links.new(surface.outputs["Emission"], out.inputs["Surface"])
        elif output == "field":
            surface = nodes.new("ShaderNodeEmission")
            surface.location = (550, 0)
            color_socket, scalar_socket = "Color", "Strength"
            links.new(surface.outputs["Emission"], out.inputs["Surface"])
        else:
            surface = nodes.new("ShaderNodeBsdfPrincipled")
            surface.location = (550, 0)
            color_socket, scalar_socket = "Base Color", "Roughness"
            links.new(surface.outputs["BSDF"], out.inputs["Surface"])

        # Texture Coordinate -> Mapping (Object space coordinates for unlit baking independence)
        coord = nodes.new("ShaderNodeTexCoord")
        coord.location = (-1300, 0)
        mapping = nodes.new("ShaderNodeMapping")
        mapping.location = (-1120, 0)
        mapping.inputs["Scale"].default_value = (scale_hint, scale_hint, scale_hint)
        links.new(coord.outputs["Object"], mapping.inputs["Vector"])

        seed_w = float(seed % 997)
        scales = spec["scale_scales"]

        # Three octaves of noise: coarse anatomy/pattern, medium rosettes/mottling, fine pebble pores
        noises = [
            _noise(nt, scales[0] * scale_hint, (-900, 320), detail=3, distortion=0.15, seed_w=seed_w),
            _noise(nt, scales[1] * scale_hint, (-900, 80), detail=5, distortion=0.08, seed_w=seed_w + 37.0),
            _noise(nt, scales[2] * scale_hint, (-900, -160), detail=6, distortion=0.0, seed_w=seed_w + 83.0),
        ]
        for node in noises:
            links.new(mapping.outputs["Vector"], node.inputs["Vector"])

        # Multiscale structure mix
        mix_coarse = nodes.new("ShaderNodeMix")
        mix_coarse.location = (-650, 100)
        mix_coarse.data_type = "FLOAT"
        mix_coarse.inputs["Factor"].default_value = 0.50
        links.new(noises[0].outputs["Fac"], mix_coarse.inputs[2])
        links.new(noises[1].outputs["Fac"], mix_coarse.inputs[3])

        mix_fine = nodes.new("ShaderNodeMix")
        mix_fine.location = (-450, 40)
        mix_fine.data_type = "FLOAT"
        mix_fine.inputs["Factor"].default_value = 0.35
        links.new(mix_coarse.outputs[0], mix_fine.inputs[2])
        links.new(noises[2].outputs["Fac"], mix_fine.inputs[3])

        # Preset-specific secondary pattern (Voronoi pebbled rosettes for skin, Wave striations for keratin)
        if preset == "smalltrotter_keratin":
            pattern = nodes.new("ShaderNodeTexWave")
            pattern.location = (-900, -420)
            pattern.wave_type = "BANDS"
            pattern.bands_direction = "Z"
            pattern.inputs["Scale"].default_value = spec["wave_scale"] * scale_hint
            pattern.inputs["Distortion"].default_value = 2.5
            pattern.inputs["Detail"].default_value = 4.0
            links.new(mapping.outputs["Vector"], pattern.inputs["Vector"])
            pattern_fac = pattern.outputs["Fac"]
        else:
            pattern = nodes.new("ShaderNodeTexVoronoi")
            pattern.location = (-900, -420)
            pattern.feature = "F1"
            pattern.distance = "EUCLIDEAN"
            pattern.inputs["Scale"].default_value = spec["voronoi_scale"] * scale_hint
            links.new(mapping.outputs["Vector"], pattern.inputs["Vector"])
            pattern_fac = pattern.outputs["Distance"]

        # Mix pattern into surface field
        mix_pattern = nodes.new("ShaderNodeMix")
        mix_pattern.location = (-250, 0)
        mix_pattern.data_type = "FLOAT"
        mix_pattern.inputs["Factor"].default_value = 0.28 if preset == "smalltrotter_keratin" else 0.32
        links.new(mix_fine.outputs[0], mix_pattern.inputs[2])
        links.new(pattern_fac, mix_pattern.inputs[3])

        # Restrained Albedo color ramp: mottling -> base -> accent
        ramp = nodes.new("ShaderNodeValToRGB")
        ramp.location = (30, 160)
        ramp.color_ramp.elements[0].position = 0.15
        ramp.color_ramp.elements[0].color = spec["mottling"]
        ramp.color_ramp.elements[1].position = 0.50
        ramp.color_ramp.elements[1].color = spec["base"]
        elem_accent = ramp.color_ramp.elements.new(0.85)
        elem_accent.color = spec["accent"]
        links.new(mix_pattern.outputs[0], ramp.inputs["Fac"])

        # Roughness map range
        rough_ramp = nodes.new("ShaderNodeMapRange")
        rough_ramp.location = (30, -120)
        rough_ramp.inputs["From Min"].default_value = 0.0
        rough_ramp.inputs["From Max"].default_value = 1.0
        rough_ramp.inputs["To Min"].default_value = spec["rough"][1]
        rough_ramp.inputs["To Max"].default_value = spec["rough"][0]
        links.new(mix_pattern.outputs[0], rough_ramp.inputs["Value"])

        # Wire outputs
        if output == "emission":
            links.new(ramp.outputs["Color"], surface.inputs[color_socket])
            surface.inputs[scalar_socket].default_value = 1.0
        elif output == "roughness":
            links.new(rough_ramp.outputs["Result"], surface.inputs[color_socket])
            surface.inputs[scalar_socket].default_value = 1.0
        elif output == "field":
            links.new(mix_pattern.outputs[0], surface.inputs[color_socket])
            surface.inputs[scalar_socket].default_value = 1.0
        else:
            bsdf = surface
            links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
            links.new(rough_ramp.outputs["Result"], bsdf.inputs["Roughness"])

            # Bump chain
            bump = nodes.new("ShaderNodeBump")
            bump.location = (260, -320)
            bump.inputs["Strength"].default_value = float(spec["bump"])
            bump.inputs["Distance"].default_value = 0.015 / max(scale_hint, 1e-3)
            links.new(mix_pattern.outputs[0], bump.inputs["Height"])
            links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

            # Subsurface scattering parameters
            sss_val = float(spec["subsurface"] if subsurface is None else subsurface)
            if "Subsurface Weight" in bsdf.inputs:
                bsdf.inputs["Subsurface Weight"].default_value = sss_val
                if "Subsurface Radius" in bsdf.inputs:
                    bsdf.inputs["Subsurface Radius"].default_value = spec["subsurface_radius"]
            elif "Subsurface" in bsdf.inputs:
                bsdf.inputs["Subsurface"].default_value = sss_val

            if "IOR" in bsdf.inputs:
                bsdf.inputs["IOR"].default_value = 1.46 if preset != "smalltrotter_keratin" else 1.52

        mat.diffuse_color = spec["base"]
        mat["stalthos_creature_preset"] = preset
        mat["stalthos_no_baked_lighting"] = True
        mat["stalthos_output"] = output
        return mat

    def assign_creature_material(obj: bpy.types.Object, material: bpy.types.Material) -> None:
        """Assign given material to object data."""
        obj.data.materials.clear()
        obj.data.materials.append(material)

    def creature_bake_target_material(
        name: str,
        preset: str,
        channel: str,
        *,
        seed: int = 0,
        scale_hint: float = 1.0,
    ) -> bpy.types.Material:
        """Generate unlit emission bake targets for albedo or roughness."""
        if channel not in {"albedo", "roughness"}:
            raise ValueError(f"Unknown creature bake channel: {channel}. Expected 'albedo' or 'roughness'.")
        out_mode = "emission" if channel == "albedo" else "roughness"
        mat = build_creature_material(name, preset, seed=seed, scale_hint=scale_hint, output=out_mode)
        mat["stalthos_bake_target"] = True
        mat["stalthos_bake_channel"] = channel
        return mat
