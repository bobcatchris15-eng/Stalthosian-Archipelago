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

## Native construction check

Run `scripts/Verify-WatershedJoin.ps1 -Duration 18`. The separate diagnostic
uses 64 cells at 2 m spacing per 128 m tile. Four central tiles stay pinned;
body-centered 3x3 neighborhoods add at most nine surrounding tiles. The fixed
route moves from (24,24) to (544,24) and back to exactly (24,24), processing the
endpoint frame. This evicts and rebuilds outer tiles without replacing the
watershed datum. The cache samples engine-owned float position/index buffers.
Reconstruction digests cover positions and normals; colors/materials are not
claimed byte-identical by that check.

The receipt measures shared float mesh edge heights/normals, original interior
2 m triangle height agreement, reconstructed mesh digests, bottom-center
contact and post-update proxy bounds. Capture diagnostics must show the marker
before the inner boundary, near the outer join and farther into coarse terrain.
The verifier rejects absent/nonfinite metrics, incorrect capture locations,
missing renderer visibility, mismatched reconstruction and an incomplete return.
Inspect all three retained images as a separate presentation check.

This is a diagnostic coordinate/terrain join only. The normal populated-world
mode still uses its original detailed patch; production population activation,
regional habitat placement, water rendering outside the watershed and dormant
population evolution remain subsequent integration work. The diagnostic uses
coarse color materials, an accelerated proxy and disabled occlusion culling;
it is engineering evidence, not final scenery, full-footprint/foot contact or
30 FPS acceptance. Analytical versus triangle height error includes the original
discontinuous reference noise; proxy support always uses rendered triangles.
