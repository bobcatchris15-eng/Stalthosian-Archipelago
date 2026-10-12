# Smalltrotter Creature Surfacing Draft

**Status:** Proposed Lookdev Draft
**Scope:** Reusable procedural material helpers for fictional Smalltrotter (dwarf titanosaur) surfaces in Blender 5.2.
**Audience:** Pipeline maintainers, geometry/rigging workers, downstream glTF baking passes.

---

## 1. Important Lookdev & Biological Disclaimers

1. **Proposed Lookdev, Not Accepted Anatomy or Final Skin:**
   These procedural swatches are a first material draft for the fictional Smalltrotter. The palettes aim at the supplied reference's warm ochre/tan dorsal skin, irregular dark-brown mottling, and pale cream underside. They do not establish final anatomy, skin, or paleontological reconstruction.
2. **Restrained Albedo & Plausible Roughness:**
   Seed variation changes the base/accent RGB channels by at most 3% multiplicatively and roughness by at most 0.025. The mottling color stays fixed so broad color separation remains stable across seeds.
3. **No Paid / Downloaded Assets:**
   All shaders are procedural node graphs authored directly in Blender 5.2 LTS using pure procedural textures (`ShaderNodeTexNoise`, `ShaderNodeTexVoronoi`, `ShaderNodeTexWave`, `ShaderNodeMapRange`, `ShaderNodeValToRGB`, and `ShaderNodeBsdfPrincipled`).

---

## 2. Architecture & API

The module is housed in `stalthos-toolkit/src/stalthos_toolkit/blender/scripts/_creature_materials.py`. It is authored to remain clean of unseeded randomness and safe for offline unit testing:

- **Offline Importability:**
  When imported outside Blender (where `bpy` is absent), the module exports `get_creature_material_spec()` and `CREATURE_PRESETS` without error. Parameter tests run offline.
- **Determinism:**
  All variations derive strictly from SHA-256 byte hashes keyed by `(seed, label, salt)`. Global `random`, `hash()`, and Blender `#frame` drivers are strictly avoided.
- **Node Graph Topology:**
  Texture coordinates are anchored via `ShaderNodeTexCoord -> Object` mapping. The shader structure is independent of scene lights, preventing directional lighting from being baked into the albedo.

### Helper Functions

```python
# Pure parameter calculation (usable with or without bpy)
spec = get_creature_material_spec(preset="smalltrotter_dorsal", seed=101)

# In Blender 5.2 (bpy present):
mat = build_creature_material(name="mat_dorsal", preset="smalltrotter_dorsal", seed=101, scale_hint=1.0)
assign_creature_material(obj, mat)

# Bake target extraction (albedo or roughness emission)
bake_mat = creature_bake_target_material(name="mat_bake_alb", preset="smalltrotter_dorsal", channel="albedo", seed=101)
```

---

## 3. Presets

| Preset Key | Description | Base Albedo (Linear RGBA) | Accent / Mottling (Linear RGB) | Roughness Range | Bump Strength | Subsurface Weight |
|---|---|---|---|---|---|---|
| `smalltrotter_dorsal` | Warm ochre/tan pebbled flank and dorsal skin | `(0.36, 0.25, 0.14, 1.0)` | `(0.48, 0.35, 0.21)` / `(0.075, 0.038, 0.018)` | 0.65 (valleys) -> 0.45 (peaks) | 0.40 | 0.08 |
| `smalltrotter_underside` | Cream countershaded throat and belly | `(0.62, 0.52, 0.38, 1.0)` | `(0.72, 0.62, 0.47)` / `(0.36, 0.27, 0.18)` | 0.58 (valleys) -> 0.38 (peaks) | 0.22 | 0.12 |
| `smalltrotter_keratin` | Dark claw and foot ungual keratin | `(0.08, 0.07, 0.06, 1.0)` | `(0.14, 0.12, 0.10)` / `(0.05, 0.05, 0.04)` | 0.42 (valleys) -> 0.22 (peaks) | 0.50 | 0.03 |

---

## 4. glTF PBR Baking Requirements

Because Wicked Engine and runtime export rely on standard glTF 2.0 PBR materials (BaseColor, MetallicRoughness, Normal, and Occlusion/Thickness), procedural 3D coordinates must be baked into 2D UV texture atlases:

1. **UV Unwrapping Requirement:**
   The downstream creature geometry worker (`creature.py` / body meshes) must supply non-overlapping, seam-optimized UV sets for body and limb meshes.
2. **Emission Bake Material Helper (not a bake pass):**
   `creature_bake_target_material()` builds an emission shader for base color or grayscale roughness. It does not create a UV map or image target, invoke Blender baking, write textures, or verify exported glTF. A bake command must supply those operations and preserve albedo without scene lighting.
3. **Normal / Tangent Space Baking (not implemented):**
   No normal-map bake is implemented here. The bump field only appears in the authored Principled material; baking it remains future pipeline work.
4. **Channel Packing (not implemented):**
   This module does not pack roughness, metallic, or occlusion textures. An export stage must create the glTF metallic-roughness texture (Green = Roughness, Blue = Metallic) and validate it.

---

## 5. Verification & Test Evidence

- **Tool & Engine Version:**
  Blender 5.2.0 LTS (Release x86-64-v2, hash `fbe6228777e7`, build 2026-07-14 01:35:40).
  Python 3.14.5, pytest 9.1.1.
- **Offline Tests:**
  `python -m pytest -q tests/test_creature_materials.py` -> 8 passed in 0.13s.
  `python -m pytest -q tests/test_authored_material_channels.py tests/test_creature_materials.py` -> 11 passed in 0.14s.
- **Render Output:**
  Blender 5.2.0 LTS rendered three material swatches on spheres. This checks shader construction and broad palette only. It is not a creature model, in-context lighting review, glTF bake/export check, or runtime visual acceptance. The PNG and editable `.blend` are under ignored `var/test-renders/creature-surfacing/` in the review worktree.

Root verification: the initial sphere capture clipped the outer spheres.
A separate camera-framed rerender now shows all three material spheres at
`var/worktrees/agy-creature-surfacing/var/test-renders/creature-surfacing/smalltrotter_swatches-framed.png`.
It is surface lookdev only: these materials have not been assigned to the
Smalltrotter mesh, baked into glTF textures or verified in Wicked.
