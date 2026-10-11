# Reference landscape fixture

`domain/reference_landscape.hpp` provides a deterministic, engine-independent
terrain and authored-placement source for a small Main Island scene. The patch
is 192 by 192 metres (`x,z` from -96 through +96), with `y` upward. Its creek
runs generally along +Z; positive X crosses from its low banks toward wooded
shoulders and a resistant west-side bluff with a caprock-like outcrop.

`ReferenceLandscape(seed)` exposes `height(x,z)`, `habitat(x,z)`,
`waterContains(x,z)`, `waterSurface(x,z)`, `inBounds(x,z)`, and an immutable
`placements()` vector. Queries outside the finite patch and non-finite values
throw `std::invalid_argument`. Water uses the terrain/surface intersection inside a bounded creek corridor, with a
slight longitudinal grade. Terrain combines smooth landform envelopes and
small deterministic seed-derived variation. Placement candidates use a stable
jittered lattice; each record supplies kind, XYZ position, uniform scale and
heading in radians. Vegetation excludes the channel, and trees occur only in
woodland habitat. Root integration can sample a mesh and instance the list.

These dimensions, relief values, habitat boundaries, densities and material
interpretations are provisional scene-authoring choices. The fixture is not a
hydrology simulation and does not establish photographic fidelity, integrated
scene acceptance, or target-hardware frame rate. Those require host integration
and player-facing rendered review.

The offline standalone check can be built with MSVC C++17 and strict warnings:

```powershell
cl /nologo /std:c++17 /W4 /WX /EHsc /permissive- /Fe:reference_landscape.exe stalthos-runtime/tests/reference_landscape.cpp
.\reference_landscape.exe
```
