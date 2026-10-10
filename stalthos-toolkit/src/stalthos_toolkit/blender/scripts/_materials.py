"""Procedural material construction for bark, frond, leaf and skin surfaces.

Two hard rules from the plan:

* **Never bake directional lighting into albedo.** The node setups below are
  built from 3D noise/Voronoi/Wave textures driven by an explicit
  ``Texture Coordinate -> Object`` mapping, so the surface structure exists
  independently of any light rig. The bake pass (see ``bake.py``) then writes
  *structure* into albedo/roughness/normal/thickness with the lighting
  contribution disabled.
* **No time-varying inputs.** No ``#frame`` drivers, no time-based noise
  (``W`` on a 4D noise is a fixed seed-derived constant, not a frame value).
"""

from __future__ import annotations

from typing import Any

import bpy

#: Multiscale structure: coarse (large forms) -> medium -> fine (microtexture).
SCALES = (2.5, 14.0, 60.0)

BARK = {"base": (0.16, 0.11, 0.075, 1.0), "accent": (0.32, 0.24, 0.17, 1.0),
        "rough": (0.92, 0.72), "bump": 0.9, "kind": "bark"}
FROND = {"base": (0.10, 0.26, 0.09, 1.0), "accent": (0.34, 0.52, 0.16, 1.0),
         "rough": (0.58, 0.34), "bump": 0.28, "kind": "frond"}
LEAF = {"base": (0.09, 0.22, 0.07, 1.0), "accent": (0.28, 0.46, 0.13, 1.0),
        "rough": (0.48, 0.26), "bump": 0.2, "kind": "leaf"}
STEM = {"base": (0.13, 0.20, 0.09, 1.0), "accent": (0.30, 0.40, 0.16, 1.0),
        "rough": (0.7, 0.5), "bump": 0.35, "kind": "stem"}
SKIN = {"base": (0.24, 0.20, 0.15, 1.0), "accent": (0.38, 0.33, 0.25, 1.0),
        "rough": (0.62, 0.42), "bump": 0.45, "kind": "skin"}

PRESETS = {"bark": BARK, "frond": FROND, "leaf": LEAF, "stem": STEM, "skin": SKIN}


def _nodes(mat: bpy.types.Material):
    return mat.node_tree, mat.node_tree.nodes, mat.node_tree.links


def new_material(name: str) -> bpy.types.Material:
    mat = bpy.data.materials.get(name)
    if mat is not None:
        return mat
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    for node in list(nt.nodes):
        if node.type in {"BSDF_PRINCIPLED", "OUTPUT_MATERIAL"}:
            nt.nodes.remove(node)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    out.location = (700, 0)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (380, 0)
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def _noise(nt, scale: float, location: tuple[float, float], detail: int = 4,
           roughness: float = 0.55, distortion: float = 0.0, seed: float = 0.0) -> bpy.types.Node:
    node = nt.nodes.new("ShaderNodeTexNoise")
    node.location = location
    node.noise_dimensions = "4D"
    node.inputs["Scale"].default_value = float(scale)
    node.inputs["Detail"].default_value = float(detail)
    node.inputs["Roughness"].default_value = float(roughness)
    if "Distortion" in node.inputs:
        node.inputs["Distortion"].default_value = float(distortion)
    # explicit, seed-derived W: a fixed constant, never a frame value
    node.inputs["W"].default_value = float(seed % 1000.0)
    return node


def build_procedural_material(
    name: str,
    preset: str,
    *,
    seed: int = 0,
    scale_hint: float = 1.0,
    subsurface: float = 0.0,
    transmission: float = 0.0,
    output: str = "principled",
) -> bpy.types.Material:
    """Build a multiscale procedural surface. No lighting is baked into albedo.

    Structure: three 4D noise octaves (coarse/medium/fine) plus a Voronoi
    crack layer for bark and a Wave banding layer; the combined field drives a
    colour ramp (base -> accent), the roughness range and the bump chain.

    ``output`` selects the surface output:
      * ``principled``   -- the authored PBR surface (what the artist sees),
      * ``emission``     -- a flat, unlit copy of the *structure only*, used as
        the albedo bake target so no light can possibly enter the image,
      * ``roughness``    -- emission of the roughness field (non-colour),
      * ``field``        -- plain field output, used by callers that want to
        wire the field into their own shader.

    The structure nodes always live in this material's own tree: Blender drops
    links that cross node trees, so the bake targets cannot borrow from the
    authored material's nodes.
    """
    spec = dict(PRESETS.get(preset, LEAF))
    mat = new_material(name)
    nt, nodes, links = _nodes(mat)
    for node in list(nodes):
        nt.nodes.remove(node)
    out = nodes.new("ShaderNodeOutputMaterial")
    out.location = (760, 0)

    # the surface node is created first so the field can be wired into it
    if output == "emission":
        surface = nodes.new("ShaderNodeEmission")
        surface.location = (400, 0)
        color_socket, scalar_socket = "Color", "Strength"
    elif output == "roughness":
        surface = nodes.new("ShaderNodeEmission")
        surface.location = (400, 0)
        color_socket, scalar_socket = "Color", "Strength"
    else:
        surface = nodes.new("ShaderNodeBsdfPrincipled")
        surface.location = (400, 0)
        color_socket, scalar_socket = "Base Color", "Roughness"
    links.new(surface.outputs[0], out.inputs["Surface"])

    coord = nodes.new("ShaderNodeTexCoord")
    coord.location = (-1200, 0)
    mapping = nodes.new("ShaderNodeMapping")
    mapping.location = (-1020, 0)
    mapping.inputs["Scale"].default_value = (scale_hint, scale_hint, scale_hint)
    links.new(coord.outputs["Object"], mapping.inputs["Vector"])

    seed_w = float(seed % 997)
    noises = [
        _noise(nt, SCALES[0] * scale_hint, (-820, 320), detail=3, distortion=0.25, seed=seed_w),
        _noise(nt, SCALES[1] * scale_hint, (-820, 60), detail=6, distortion=0.1, seed=seed_w + 31.0),
        _noise(nt, SCALES[2] * scale_hint, (-820, -200), detail=8, distortion=0.0, seed=seed_w + 71.0),
    ]
    for node in noises:
        links.new(mapping.outputs["Vector"], node.inputs["Vector"])

    vor = nt.nodes.new("ShaderNodeTexVoronoi")
    vor.location = (-820, -460)
    vor.feature = "DISTANCE_TO_EDGE"
    vor.inputs["Scale"].default_value = 9.0 * scale_hint
    links.new(mapping.outputs["Vector"], vor.inputs["Vector"])

    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.location = (-820, -700)
    wave.wave_type = "BANDS"
    wave.bands_direction = "Z"
    wave.inputs["Scale"].default_value = 14.0 * scale_hint
    wave.inputs["Distortion"].default_value = 6.0
    links.new(mapping.outputs["Vector"], wave.inputs["Vector"])

    mix_structure = nodes.new("ShaderNodeMix")
    mix_structure.location = (-560, 60)
    mix_structure.data_type = "FLOAT"
    mix_structure.inputs["Factor"].default_value = 0.55
    links.new(noises[0].outputs["Fac"], mix_structure.inputs[2])
    links.new(noises[1].outputs["Fac"], mix_structure.inputs[3])

    mix_fine = nodes.new("ShaderNodeMix")
    mix_fine.location = (-360, 0)
    mix_fine.data_type = "FLOAT"
    mix_fine.inputs["Factor"].default_value = 0.3
    links.new(mix_structure.outputs[0], mix_fine.inputs[2])
    links.new(noises[2].outputs["Fac"], mix_fine.inputs[3])

    mix_crack = nodes.new("ShaderNodeMix")
    mix_crack.location = (-160, 0)
    mix_crack.data_type = "FLOAT"
    mix_crack.inputs["Factor"].default_value = 0.25 if preset == "bark" else 0.08
    links.new(mix_fine.outputs[0], mix_crack.inputs[2])
    links.new(vor.outputs["Distance"], mix_crack.inputs[3])

    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.location = (40, 120)
    ramp.color_ramp.elements[0].position = 0.28
    ramp.color_ramp.elements[0].color = spec["base"]
    ramp.color_ramp.elements[1].position = 0.74
    ramp.color_ramp.elements[1].color = spec["accent"]
    links.new(mix_crack.outputs[0], ramp.inputs["Fac"])

    rough_ramp = nodes.new("ShaderNodeMapRange")
    rough_ramp.location = (40, -160)
    rough_ramp.inputs["From Min"].default_value = 0.0
    rough_ramp.inputs["From Max"].default_value = 1.0
    rough_ramp.inputs["To Min"].default_value = spec["rough"][1]
    rough_ramp.inputs["To Max"].default_value = spec["rough"][0]
    links.new(mix_crack.outputs[0], rough_ramp.inputs["Value"])

    if output in {"emission", "roughness"}:
        source_socket = ramp.outputs["Color"] if output == "emission" else rough_ramp.outputs["Result"]
        links.new(source_socket, surface.inputs[color_socket])
        surface.inputs[scalar_socket].default_value = 1.0
        surface.location = (400, 120)
    elif output == "field":
        links.new(mix_crack.outputs[0], surface.inputs[color_socket])
    else:
        bsdf = surface
        links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
        links.new(rough_ramp.outputs["Result"], bsdf.inputs["Roughness"])

    bump = nodes.new("ShaderNodeBump")
    bump.location = (180, -420)
    bump.inputs["Strength"].default_value = float(spec["bump"])
    bump.inputs["Distance"].default_value = 0.02 / max(scale_hint, 1e-3)
    links.new(mix_crack.outputs[0], bump.inputs["Height"])
    normal_out = bump.outputs["Normal"]

    wave_bump = nodes.new("ShaderNodeBump")
    wave_bump.location = (180, -620)
    wave_bump.inputs["Strength"].default_value = 0.25 if preset == "bark" else 0.1
    links.new(wave.outputs["Fac"], wave_bump.inputs["Height"])
    links.new(normal_out, wave_bump.inputs["Normal"])
    normal_out = wave_bump.outputs["Normal"]

    if output == "principled":
        if "Subsurface Weight" in bsdf.inputs:
            bsdf.inputs["Subsurface Weight"].default_value = float(subsurface)
            if "Subsurface Radius" in bsdf.inputs:
                bsdf.inputs["Subsurface Radius"].default_value = (0.02, 0.012, 0.008)
        if "Transmission Weight" in bsdf.inputs:
            bsdf.inputs["Transmission Weight"].default_value = float(transmission)
        if "IOR" in bsdf.inputs:
            bsdf.inputs["IOR"].default_value = 1.42 if preset in {"frond", "leaf"} else 1.45
        links.new(normal_out, bsdf.inputs["Normal"])

    mat.diffuse_color = spec["base"]
    mat["stalthos_structure"] = preset
    mat["stalthos_no_baked_lighting"] = True
    mat["stalthos_output"] = output
    return mat


def assign(obj: bpy.types.Object, material: bpy.types.Material) -> None:
    obj.data.materials.clear()
    obj.data.materials.append(material)


def structure_emission_material(name: str, preset: str, *, seed: int = 0,
                                scale_hint: float = 1.0) -> bpy.types.Material:
    """A flat, unlit copy of the surface *structure* for baking.

    Emission-only: no light can enter, so the baked image is pure albedo
    structure. This is the mechanism that guarantees no directional lighting
    ends up in the albedo map (plan requirement).
    """
    mat = build_procedural_material(name, preset, seed=seed, scale_hint=scale_hint,
                                    output="emission")
    mat["stalthos_bake_target"] = True
    return mat


def structure_roughness_material(name: str, preset: str, *, seed: int = 0,
                                  scale_hint: float = 1.0) -> bpy.types.Material:
    """Emission of the roughness field, non-color, for the roughness bake."""
    mat = build_procedural_material(name, preset, seed=seed, scale_hint=scale_hint,
                                    output="roughness")
    mat["stalthos_bake_target"] = True
    return mat


def structure_thickness_material(name: str, *, seed: int = 0, scale_hint: float = 1.0) -> bpy.types.Material:
    """Emission of the geometric thickness attribute, for the thickness bake.

    Blender 5.2's Solidify modifier does not expose a readable ``thickness``
    attribute, so the bake script computes a real per-vertex thickness estimate
    (local cross-section radius / blade thickness) into a
    ``stalthos_thickness`` mesh attribute. This material reads that attribute,
    so the baked thickness map is geometric rather than guessed. The attribute
    is preserved in the saved master for downstream use.
    """
    mat = new_material(name)
    nt, nodes, links = _nodes(mat)
    for node in list(nodes):
        nt.nodes.remove(node)
    out = nodes.new("ShaderNodeOutputMaterial")
    out.location = (600, 0)
    emit = nodes.new("ShaderNodeEmission")
    emit.location = (380, 0)
    links.new(emit.outputs["Emission"], out.inputs["Surface"])
    attr = nodes.new("ShaderNodeAttribute")
    attr.location = (-200, 0)
    attr.attribute_name = "stalthos_thickness"
    scale = nodes.new("ShaderNodeMapRange")
    scale.location = (60, 0)
    # the attribute is stored in metres; map a 1 cm reference range to 0..1
    scale.inputs["From Min"].default_value = 0.0
    scale.inputs["From Max"].default_value = 0.01
    scale.inputs["To Min"].default_value = 0.0
    scale.inputs["To Max"].default_value = 1.0
    scale.clamp = True
    links.new(attr.outputs["Fac"], scale.inputs["Value"])
    links.new(scale.outputs["Result"], emit.inputs["Color"])
    mat["stalthos_bake_target"] = True
    return mat
