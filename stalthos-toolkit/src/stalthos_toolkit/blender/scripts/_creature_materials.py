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
import math
from array import array
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


def _mix32(value: int) -> int:
    """Stable unsigned 32-bit mixer (independent of Python hash/random state)."""
    value = (value + 0x9E3779B9) & 0xFFFFFFFF
    value = ((value ^ (value >> 16)) * 0x85EBCA6B) & 0xFFFFFFFF
    value = ((value ^ (value >> 13)) * 0xC2B2AE35) & 0xFFFFFFFF
    return (value ^ (value >> 16)) & 0xFFFFFFFF


def _grid_value(x: int, y: int, cells: int, seed: int) -> float:
    key = (int(seed) & 0xFFFFFFFF) ^ ((x % cells) * 0x1F123BB5) ^ ((y % cells) * 0x5F356495) ^ (cells * 0x6C8E9CF5)
    return _mix32(key) / 4294967295.0


def _periodic_noise(u: float, v: float, cells: int, seed: int) -> float:
    """Smooth, periodic value noise for texture maps; returns [0, 1]."""
    x, y = u * cells, v * cells
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3.0 - 2.0 * fx), fy * fy * (3.0 - 2.0 * fy)
    a = _grid_value(ix, iy, cells, seed)
    b = _grid_value(ix + 1, iy, cells, seed)
    c = _grid_value(ix, iy + 1, cells, seed)
    d = _grid_value(ix + 1, iy + 1, cells, seed)
    return (a * (1.0 - fx) + b * fx) * (1.0 - fy) + (c * (1.0 - fx) + d * fx) * fy


def _spatial_noise_2d(x: float, y: float, frequency: float, seed: int) -> float:
    """Non-periodic, smoothly interpolated position noise for face masks."""
    gx, gy = x * frequency, y * frequency
    ix, iy = math.floor(gx), math.floor(gy)
    fx, fy = gx - ix, gy - iy
    fx, fy = fx * fx * (3.0 - 2.0 * fx), fy * fy * (3.0 - 2.0 * fy)
    def at(a: int, b: int) -> float:
        key = (int(seed) & 0xFFFFFFFF) ^ ((a * 0x1F123BB5) & 0xFFFFFFFF) ^ ((b * 0x5F356495) & 0xFFFFFFFF)
        return _mix32(key) / 4294967295.0
    a, b = at(ix, iy), at(ix + 1, iy)
    c, d = at(ix, iy + 1), at(ix + 1, iy + 1)
    return (a * (1.0 - fx) + b * fx) * (1.0 - fy) + (c * (1.0 - fx) + d * fx) * fy


def _surface_sample(surface: str, u: float, v: float, seed: int) -> tuple[tuple[float, float, float], float]:
    """Sample a seeded UV color/roughness field in linear RGB for tests and maps."""
    coarse = _periodic_noise(u, v, 7, seed ^ 0xA341316C)
    medium = _periodic_noise(u, v, 27, seed ^ 0xC8013EA4)
    fine = _periodic_noise(u, v, 109, seed ^ 0xAD90777D)
    rough_field = _periodic_noise(u, v, 53, seed ^ 0x7E95761E)
    if surface == "hide":
        # Keep the UV atlas map locally quiet; broad irregular markings are
        # assigned from mesh position below so Smart-UV island boundaries do
        # not become conspicuous color blocks.
        tint = (medium - 0.5) * 0.075 + (fine - 0.5) * 0.025
        color = (0.43 + tint, 0.285 + tint * 0.76, 0.135 + tint * 0.43)
        roughness = 0.57 + rough_field * 0.20
    elif surface == "mottle":
        tint = (medium - 0.5) * 0.055
        color = (0.135 + tint, 0.070 + tint * 0.63, 0.032 + tint * 0.38)
        roughness = 0.61 + rough_field * 0.16
    elif surface == "ventral":
        field = coarse * 0.36 + medium * 0.64
        palette = ((0.34, 0.245, 0.145), (0.51, 0.39, 0.245),
                   (0.66, 0.535, 0.365), (0.74, 0.62, 0.445))
        color = palette[0] if field < 0.13 else palette[1] if field < 0.29 else palette[2] if field < 0.86 else palette[3]
        if fine > 0.97 and field < 0.55:
            color = palette[0]
        roughness = 0.52 + rough_field * 0.17
    elif surface == "keratin":
        streak = (0.5 + 0.5 * math.sin(v * 2.0 * math.pi * 46.0 + coarse * 3.4))
        shade = 0.035 + 0.032 * (0.60 * streak + 0.40 * medium)
        color = (shade * 1.0, shade * 0.74, shade * 0.53)
        roughness = 0.38 + rough_field * 0.17
    elif surface == "bristle":
        streak = (0.5 + 0.5 * math.sin(v * 2.0 * math.pi * 61.0 + coarse * 2.7))
        shade = 0.045 + 0.055 * (0.55 * streak + 0.45 * medium)
        color = (shade * 1.0, shade * 0.55, shade * 0.29)
        roughness = 0.68 + rough_field * 0.16
    elif surface == "eye":
        shade = 0.012 + 0.014 * coarse
        color = (shade, shade * 0.52, shade * 0.25)
        roughness = 0.105 + 0.035 * rough_field
    else:
        raise ValueError(f"Unknown Smalltrotter surface: {surface}")
    return tuple(round(float(c), 6) for c in color), round(float(roughness), 6)


def _linear_to_srgb(value: float) -> float:
    return 12.92 * value if value <= 0.0031308 else 1.055 * (value ** (1.0 / 2.4)) - 0.055


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

    _EXPORT_SURFACES = ("hide", "ventral", "keratin", "bristle", "eye", "mottle")

    def _image_pair(surface: str, seed: int, size: int = 256):
        """Create packed UV base-color and roughness maps for glTF export."""
        base_name = f"ST_Smalltrotter_{surface}_BaseColor_s{int(seed)}"
        rough_name = f"ST_Smalltrotter_{surface}_Roughness_s{int(seed)}"
        base = bpy.data.images.new(base_name, width=size, height=size, alpha=False, float_buffer=False)
        rough = bpy.data.images.new(rough_name, width=size, height=size, alpha=False, float_buffer=False)
        base.colorspace_settings.name = "sRGB"
        rough.colorspace_settings.name = "Non-Color"
        base_pixels = array("f")
        rough_pixels = array("f")
        # Evaluate at texel centers. Periodic noise avoids a hard boundary when
        # UV islands cross the image edge; no Blender or Python global RNG.
        for y in range(size):
            v = (y + 0.5) / size
            for x in range(size):
                u = (x + 0.5) / size
                color, r = _surface_sample(surface, u, v, seed)
                base_pixels.extend((_linear_to_srgb(color[0]), _linear_to_srgb(color[1]),
                                    _linear_to_srgb(color[2]), 1.0))
                rough_pixels.extend((r, r, r, 1.0))
        base.pixels.foreach_set(base_pixels)
        rough.pixels.foreach_set(rough_pixels)
        base.pack()
        rough.pack()
        return base, rough

    def _export_material(surface: str, seed: int) -> bpy.types.Material:
        base_image, rough_image = _image_pair(surface, seed)
        mat = bpy.data.materials.new(f"MAT_Smalltrotter_{surface.title()}_PBR")
        mat.use_nodes = True
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        nodes.clear()
        out = nodes.new("ShaderNodeOutputMaterial")
        out.location = (520, 0)
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        bsdf.location = (260, 0)
        links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
        base = nodes.new("ShaderNodeTexImage")
        base.name = "Export_BaseColor"
        base.image = base_image
        base.location = (-260, 120)
        rough = nodes.new("ShaderNodeTexImage")
        rough.name = "Export_Roughness"
        rough.image = rough_image
        rough.location = (-260, -120)
        links.new(base.outputs["Color"], bsdf.inputs["Base Color"])
        links.new(rough.outputs["Color"], bsdf.inputs["Roughness"])
        if surface == "eye":
            bsdf.inputs["IOR"].default_value = 1.46
        mat.diffuse_color = (*_surface_sample(surface, 0.5, 0.5, seed)[0], 1.0)
        mat["stalthos_surface_class"] = surface
        mat["stalthos_texture_seed"] = str(int(seed))
        mat["stalthos_export_maps"] = "baseColor=sRGB;roughness=Non-Color;UV"
        return mat

    def _uv_unwrap_export(obj: bpy.types.Object) -> None:
        """Add a packed, deterministic UV layout without changing mesh topology."""
        if not obj.data.polygons:
            return
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        if bpy.context.object.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(island_margin=0.004, area_weight=0.0,
                                 correct_aspect=True, scale_to_bounds=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        if obj.data.uv_layers.active:
            obj.data.uv_layers.active.name = "UVMap"
            obj.data.uv_layers.active.active_render = True

    def _assign_slots(obj: bpy.types.Object, materials, indices=None) -> None:
        obj.data.materials.clear()
        for material in materials:
            obj.data.materials.append(material)
        if indices is not None:
            for poly, index in zip(obj.data.polygons, indices):
                poly.material_index = int(index)
        _uv_unwrap_export(obj)

    def assign_smalltrotter_export_materials(
        body: bpy.types.Object,
        detail: bpy.types.Object,
        eyes: bpy.types.Object | None,
        ridge: bpy.types.Object | None,
        *,
        seed: int,
        params: dict[str, Any],
    ) -> dict[str, Any]:
        """Assign image-backed PBR slots and UVs to the Smalltrotter proxy only.

        The body face partition colors ventral-facing trunk/throat areas while
        retaining the generated vertices, polygons, and rig-source metadata.
        Every map is packed into the .blend so the subsequent GLB embeds it.
        """
        mats = {surface: _export_material(surface, seed) for surface in _EXPORT_SURFACES}
        trunk_len = float(params["trunk_length_m"])
        trunk_depth = float(params["trunk_depth_m"])
        trunk_width = float(params["trunk_width_m"])
        neck_len = float(params["neck_length_m"])
        head_len = float(params["head_length_m"])
        stance = float(params["stance_height_m"])
        indices = []
        ventral_faces = 0
        keratin_faces = 0
        mottle_faces = 0
        front_foot_x = trunk_len * 0.30 + head_len * 0.14
        rear_foot_x = -trunk_len * 0.18 + head_len * 0.14
        foot_x_radius = max(head_len * 0.68, 0.16)
        for poly in body.data.polygons:
            c = poly.center
            foot = (c.z <= 0.24 and abs(c.y) >= trunk_width * 0.20 and
                    min(abs(c.x - front_foot_x), abs(c.x - rear_foot_x)) <= foot_x_radius)
            trunk_underside = (-trunk_len * 0.56 <= c.x <= trunk_len * 0.56 and
                               c.z <= stance + trunk_depth * 0.25 and poly.normal.z < -0.20)
            lower_throat = (c.x >= trunk_len * 0.24 and c.x <= trunk_len * 0.95 and
                            c.z <= stance + trunk_depth * 0.05 + neck_len * 0.13 and
                            poly.normal.z < 0.0)
            spot = (_spatial_noise_2d(c.x, c.z, 2.7, seed ^ 0x12AFD64D) * 0.68 +
                    _spatial_noise_2d(c.x, c.z, 6.3, seed ^ 0x91E10DA5) * 0.32)
            mottled = (c.x < trunk_len * 0.80 and c.z > stance - trunk_depth * 0.60 and
                       not foot and spot < 0.68)
            if foot:
                indices.append(2)
                keratin_faces += 1
            elif trunk_underside or lower_throat:
                indices.append(1)
                ventral_faces += 1
            elif mottled:
                indices.append(3)
                mottle_faces += 1
            else:
                indices.append(0)
        _assign_slots(body, (mats["hide"], mats["ventral"], mats["keratin"], mats["mottle"]), indices)
        _assign_slots(detail, (mats["keratin"],))
        if eyes is not None:
            _assign_slots(eyes, (mats["eye"],))
        if ridge is not None:
            _assign_slots(ridge, (mats["bristle"],))
        return {
            "format": "glTF metallic-roughness PBR",
            "uv_map": "UVMap",
            "texture_size": 256,
            "seed": int(seed),
            "surfaces": list(_EXPORT_SURFACES),
            "ventral_face_count": ventral_faces,
            "dark_foot_face_count": keratin_faces,
            "dorsal_mottle_face_count": mottle_faces,
            "packed_images": sum(1 for material in mats.values() for node in material.node_tree.nodes
                                  if node.type == "TEX_IMAGE" and node.image and node.image.packed_file),
            "topology_preserved": True,
        }
