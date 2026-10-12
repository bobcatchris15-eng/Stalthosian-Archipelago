# Basketfern frond form

The `tree_frond_crown` generator uses the fern rachis and paired-pinna builder,
with a profile-specific short stout support and elevated radial crown. Mature
dimensions come from the definition's stage height, stem diameter and foliage
length; the pinna length follows its declared ratio. This replaces the former
generic recursively branching tree silhouette for this generator alone.

The generated shape is a visual interpretation of the fictional Basketfern
definition. Its marattioid analogy is speculative and does not assert a real
taxon or a verified anatomical reconstruction. Generic `compound_frond` and
other fern profiles retain their existing ground-origin behavior.

Review the generated mature seed-42 neutral and turntable views for an open,
radial vase crown, readable rachises, paired pinnae, and a short stout base.
Geometry statistics alone do not establish visual acceptance.

Seed 42, mature, Blender 5.2.0 produced 9 fronds, 612 pinnae (306 pairs), a 1.62 m
crown support, 18,518 triangles and a 3.51 m maximum height. The baked 512 px
neutral turntable confirms the rachises begin within the support cap and the
open crown mixes upright inner fronds with arching outer fronds. Pinnae extend
to 98% of each rachis. This remains a provisional visual candidate for parent
review, not an accepted final plant form.

Parent independently generated, baked, rendered and exported the final scoped
source in `var/basketfern-final-verified-asset`. Latest bake/render artifact hashes
match; bake enriches the shared generation manifest and supersedes its old digest.
The exported GLB verifies 2 materials and 6 PBR texture bindings. Actual Wicked
run `20261012-160706-ada921ce` imports 16 mesh objects across 8 plant instances.
The final root offline suite passed 1987 tests, with 2 skipped, in 590.94 s;
all three changed Python source/test hashes remained stable throughout that run.
Generic fern path initialization and random length variation remain unchanged.

This is a provisional form improvement. Plain support, coarse pinnae and dark
leaf undersides remain visible. Close-view native presentation, all growth stages,
habitat composition and full visual acceptance remain pending.
