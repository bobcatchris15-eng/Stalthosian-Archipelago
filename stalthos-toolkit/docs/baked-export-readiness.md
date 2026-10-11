# Baked material export readiness

**Status: Decided bridge; actual Blender export verified. Visual acceptance remains To test.**

`bake_materials` writes one baked image set per generated mesh object. Its saved
derived `.baked.blend` now contains image-backed Principled materials built
from albedo, roughness, and tangent-space normal images. The images are packed
into the blend. Original source material slot count and polygon material-slot
indices are retained; each slot receives a baked material carrying that
object's baked maps. The editable source blend is not modified. Thickness is
retained as a separate geometric bake and is not presented as a glTF PBR
channel.

`blender/scripts/export_baked.py` exports that derived blend and validates the
resulting GLB JSON before reporting success. It checks that each exported
material points to the expected per-channel image and that each material-using
primitive retains `TEXCOORD_0`. The Python validator is covered without Blender
by `tests/test_baked_export.py`.

The bake stage registers its verified derived master on an existing parent
asset manifest, including the input source digest and seed. `generate_lods`
selects that explicitly registered source before the original master and
never selects a collision proxy or previous generated LOD as source.
The derived blend lives directly under `intermediate/bakes/`, named
`{asset_id}{stage_suffix}.baked.blend`; textures live in its asset subdirectory.
A separate texture-set manifest remains separate from the geometry parent.

Actual Blender 5.2 execution on Softbough completed all four 512-pixel bake
passes for two objects. The independent baked exporter verified two materials
and six image bindings in the GLB. Quantized UV accessors and embedded image bytes now survive the four-level LOD chain. Texture appearance, seams and
padding, color-space behavior and target-engine visuals remain acceptance work.
These results do not establish visual acceptance or the target 30 FPS floor.
