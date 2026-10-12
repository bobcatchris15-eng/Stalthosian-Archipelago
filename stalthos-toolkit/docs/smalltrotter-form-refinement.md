# Smalltrotter form refinement receipt

This reference-specific blockout pass addresses the visible gaps in the seeded
neutral render: conical limbs, an armored-spike crest, oversized hard color
islands, and a weak head silhouette. The Smalltrotter profile alone receives
these changes; generic creature profiles continue through the prior path.

The limbs retain their authored upper/lower segment lengths, joint points,
source serialization, and explicit foot-pad vertex indices. Their radius profile
stays fuller through the knee and ankle, with restrained shoulder and haunch
forms. The crest uses seeded irregular placement with shorter rearward leaning
fibers along the nape, back, and tail. Dark hide variation is generated in the
packed UV color map, while body face classes are reserved for downward belly,
front-facing lower throat, feet, and eyes. The resulting image maps remain
packed for glTF embedding.

The belly mask follows the serialized spine centerline and selects only faces
well below that local axis with strongly downward normals. The throat mask uses
the serialized neck chain and forward-facing normals above its resolved root.
The ellipsoid helper's quad winding was reversed after a numeric normal check
showed the original faces pointed inward; vertex locations and pad-index
metadata are unchanged.

Visual review used the supplied `references/creatures/smalltrotter/user-primary.png`
and the prior `independent_juvenile_neutral_090.png`, followed by new broadside
and front turntable captures. The blockout reads closer in mass and coloring,
but still has intersecting proxy forms at the knees/pads, simplified facial
features, and a crest that remains coarse at turntable resolution. It is not a
photoreal asset or a claim of reference parity.

## Verification

Seed 42, `independent_juvenile`, Blender 5.2.0 LTS. Build and rig export are run
through the toolkit CLI into the ignored workspace
`var/smalltrotter-form-refinement-asset/`. The rig command must report passing
walk-cycle loop, root motion, geometric contact, touchdown foot lock, and GLB
animation checks. Contact tests and authored bone lengths were not relaxed.
Triangle budget remains below 50,000.

Final artifact hashes and capture paths are recorded in the accompanying
`smalltrotter-form-refinement-receipt.json` after the final build, rig export,
and comparison capture complete.
