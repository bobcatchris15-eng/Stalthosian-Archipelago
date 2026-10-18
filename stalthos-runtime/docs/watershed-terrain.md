# Watershed terrain overlay

Decided: `world::WatershedTerrain` supplies one immutable seed-based world-space
height field. Its default lattice is 16 cells at 8 m spacing (128 m tiles), with
1 m finite-difference normals. The constructor also accepts the existing bounded
`TerrainTileConfig` and a finite outer half extent, defaulting to 256 m.

For every coordinate with `abs(x) <= 96` and `abs(z) <= 96`, `height` returns
`ReferenceLandscape(seed).height(x,z)` directly, including the four borders and
corners. There is no position, height-datum or save migration. Existing water,
actor poses and resources retain their coordinates and Y values. This module
does not create water or move actors.

Outside that square, the reference height is extended by clamping each query
coordinate to the 96 m square. A separable quintic weight blends this extension
to the unchanged `Archipelago(seed).elevation(x,z)`. Each axis weight is one at
96 m and zero at `outerExtent - normalStep`. Thus an entirely coarse halo at
least one finite-difference step wide precedes the outer boundary. Height and
the sampler's numerical normal agree exactly with `TerrainTiles` on all four
outer edges, including corners. The formula changes no coarse source code.

Normals always use finite differences of this same composite height function,
including one-sided differences at the world perimeter. The reference noise
uses discontinuous cells, so this is not a claim of C1 continuity inside the
fixture or along the clamped reference extension. The quintic weight converges
smoothly to zero at the outer transition end. The blend can produce steep
slopes because the reference watershed and coarse island have different height
datums. Native traversal and presentation still require assessment.

`sample` returns `world::TerrainVertex`; moisture uses the unchanged coarse
moisture field, and `sea` is derived from composite height <= 0. Neither value
replaces the existing local creek/habitat rules. `build`, `bodyTile`,
`containsTile`, `neighborhood`, `config` and `spanMeters` match the lattice API.
All vertices use global integer lattice indices, preserving exact shared-edge
positions and normals even for negative tile keys and nonbinary spacing. There
is no cache or order-dependent state. Query/configuration limits and complete
tile guards remain the existing bounded `TerrainTiles` rules. Overlay extent
must exceed `96 + normalStep` and not exceed the coarse world bound.

Verified by the standalone `watershed_terrain.cpp` suite: exact reference
interior/border heights across three seeds, all four outer-edge normals/heights,
custom extent and nonbinary spacing, adjacent tile identity and build-order
reconstruction, finite transition samples, sampler-derived normals and invalid
configurations/queries. This establishes domain sampler behavior; it does not
establish native rendering, triangle-contact quality or frame-rate acceptance.
