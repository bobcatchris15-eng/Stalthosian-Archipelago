# Smalltrotter image-based surfacing draft

The Smalltrotter reference profile receives six UV image-backed glTF PBR
surface classes: ochre hide, pale ventral hide, dark dorsal mottles, keratin
feet/toes, brown bristle fibers, and glossy dark eyes/nostrils. Each class has
an sRGB base-color PNG and a Non-Color roughness PNG packed into the source
blend and embedded by Blender's glTF exporter. The generated `UVMap` is added
without changing mesh vertices or faces; existing pad-vertex index metadata
and rig-source data remain intact. Other creature profiles continue using the
previous material path.

Map fields and dorsal spot placement are deterministic from the explicit
toolkit seed. Body masks place the cream underside on trunk/throat polygons,
keratin on low foot pads, and irregular dark-brown spots over the dorsal and
upper flank. UV island scale and texture repeat have not been calibrated
against a measured skin scale. Review renders use the toolkit's fixed neutral
EEVEE light and are draft checks, not material or photorealism acceptance.

The head, body proportions, toe geometry, and bristles remain generated proxy
geometry. This change establishes exportable material maps and placement only;
it does not establish anatomical accuracy, animation acceptance, or target
runtime appearance. Reference image rights remain governed by the species
definition and are not altered here.
