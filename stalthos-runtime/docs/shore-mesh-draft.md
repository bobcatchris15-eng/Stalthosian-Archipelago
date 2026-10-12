# Shore mesh draft

`domain/shore_mesh.hpp` builds host-independent water surface and submerged bed triangles over the finite `ReferenceLandscape` patch. Call `generateShoreMesh(landscape, resolution)`; resolution must be finite and between 0.1 m and 10 m. The generator rejects a request before sampling if its square grid would exceed 65,536 cells. At most 16 material vertices and 24 indices per cell are permitted by hard output limits.

Grid samples classify wet/dry with `ReferenceLandscape::waterContains`. Each cell is split into two triangles and clipped against those sampled classifications. Alternating diagonal wet corners are split along the dry diagonal so disconnected wet samples do not get joined by a polygon fan. Crossing locations use 28 deterministic bisection steps on the wet/dry predicate and canonical shared grid-edge keys. Adjacent triangles therefore reuse the same crossing coordinates and material vertices.

The crossing is a bounded approximation, not an exact analytic water/terrain intersection. `ReferenceLandscape::height` contains step noise, so the predicate can change discontinuously along an edge; this method does not claim to recover a unique analytic root or exact shoreline. The mesh is suitable for bounded draft presentation and requires rendered review in the host.

Coordinates are meters in the 192 m square patch, with Y up. The domain convention checks the conventional normal `(p1 - p0) × (p2 - p0)`: both the visible water surface and creek bed triangles face +Y so top-down rendering can show the substrate through the water. Degenerate projected triangles are omitted. Surface and bed share identical X/Z vertex coordinates along their generated seam while retaining their own heights and material tags. The current Wicked host adapter computes normals with the reversed cross-product order `(p2 - p0) × (p1 - p0)`; reverse triangle indices in that adapter if it needs the same upward-facing result. These domain tests do not claim that unchanged indices produce upward Wicked normals.

Standalone offline verification:

```powershell
cl /nologo /std:c++17 /O2 /W4 /WX /EHsc /permissive- /Fe:var\tests\shore_mesh.exe stalthos-runtime\tests\shore_mesh.cpp
var\tests\shore_mesh.exe
```

This draft does not claim final visual acceptance, exact hydrology, analytic shoreline intersections, or target-frame-rate acceptance. It does not wire meshes or materials into the Wicked Engine host.
