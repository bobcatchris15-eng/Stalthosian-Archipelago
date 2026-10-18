# Coarse terrain tile sampling contract

Status: **Decided**, tested domain contract and bounded native coarse-tile probe.
Joining the existing 192 m watershed, integrated population streaming, visual
quality and target frame rate remain **To test**. This module does not replace
the reference landscape.

`domain/terrain_tiles.hpp` is a header-only adapter over `world::Archipelago`.
Construct one immutable `world::TerrainTiles(seed, config)` per terrain source.
`TerrainTileConfig{16, 8.0, 1.0}` gives 128 m tiles with 16 cells per side and
source finite-difference normals over a 1 m step. Defaults are 32 cells, 8 m
spacing, 1 m normal step (256 m tiles). Neighboring tiles must use the same seed
and configuration. Mixed resolutions/spacing and LOD stitching are not covered.

Public operations:

- `build(TerrainTileKey{x,z})` returns `TerrainTile{key,cells,vertices}`. Each
  vertex has `position`, `normal`, `moisture` and `sea`. There are `(cells+1)^2`
  vertices, z-major with x varying fastest. Engine code owns triangle topology,
  winding, material bindings and collision/contact representation.
- `sample(x,z)` samples source elevation and moisture at exactly those world
  coordinates. `sea` is `elevation <= 0`, using the same elevation result.
- `bodyTile(Vec3 body)` selects the half-open tile containing the body's x/z
  coordinates using `floor`, including negative coordinates. All three body
  coordinates must be finite; finite body altitude does not change selection.
- `neighborhood(body,radius=1)` returns tile keys in z-major/x-fast order, bounded
  to radius 0..4 (at most 81 keys). This only selects keys; it does not generate
  every mesh or individual population. The caller supplies the animal body,
  rather than a camera, and owns cache eviction/physical activation policy.
- `containsTile(key)`, `spanMeters()` and `config()` expose validity and layout.

Each world coordinate is calculated as `(tileIndex*cells + localIndex)*spacing`
from a signed integer global lattice. Origin-plus-offset floating-point sums are
avoided. Shared edges therefore have identical coordinates, heights, normals,
moisture and masks regardless of tile visitation order, including negative tiles
and non-binary spacing. Determinism applies to the same toolchain/platform and
source version; no cross-platform transcendental bit identity is promised.

Normals are normalized `(-dh/dx,1,-dh/dz)` from the source elevation field, not
from an individual tile's triangle faces. Difference sample coordinates are
clamped at the source world's +/-14000 m boundaries; the denominator uses the
actual sampled interval, giving one-sided derivatives at the exact perimeter.
This also preserves shared-edge normals at the world perimeter. Normals need
not equal the normals of coarse triangles; engine contact must use a deliberate
choice of sampled field or rendered triangles and verify that choice natively.

Bounds and errors:

- Cells are 1..256; spacing is finite 0.25..256 m; tile span is at most 4096 m;
  normal step is finite 0.01..16 m. Invalid configuration throws
  `std::invalid_argument` before allocation.
- Tile indices are checked against a conservative +/-1000000 bound before
  multiply/add, then the complete tile must fit within source world bounds.
  Rejected builds throw `std::out_of_range`, including extreme int64 inputs.
  At most 66049 vertices are allocated per tile; callers still own the total
  resident mesh budget.
- Sampling validates coordinates through the source field: nonfinite values
  throw `std::invalid_argument`; coordinates outside the world throw
  `std::out_of_range`.
- Neighborhoods omit tiles outside complete source bounds. A body whose tile
  would extend outside source bounds throws `std::out_of_range`; the leftover
  partial world perimeter is not silently extrapolated. At an exact positive
  tile boundary the body enters the next tile, which may be outside the domain.
- Invalid radius and nonfinite bodies throw `std::invalid_argument`; body bounds
  are checked before conversion to integer tile coordinates. A large finite
  coordinate cannot overflow that conversion.

`tests/terrain_tiles.cpp` checks exact shared edges on both axes over negative
and positive tile keys, non-binary spacing, reconstruction and visit order,
source normals/moisture/sea consistency, source boundary derivatives, body
boundary transitions, clipped neighborhoods, extreme indices, NaN/infinity,
invalid configurations and minimum/maximum grids. It is standalone C++17 and
needs no network, Blender or Wicked. It emits one JSON result on success.

Example standalone verification from the worktree, in an MSVC developer shell:

```powershell
cl /nologo /std:c++17 /EHsc /W4 /WX /permissive- /O2 stalthos-runtime/tests/terrain_tiles.cpp /Fe:var/terrain-test/terrain_tiles.exe /Fo:var/terrain-test/terrain_tiles.obj
./var/terrain-test/terrain_tiles.exe
```

Observed output: `{"status":"success","suite":"terrain_tiles","groups":3}`.
Domain success does not establish streamed playability or population evolution.

## Optional native construction route

`engine/terrain_stream_probe.hpp` consumes this contract in a separate Wicked
construction mode. Run `scripts/Run.ps1 -TerrainStreamProbe -Interactive` for
W/A/S/D traversal (20 m/s, Escape exits), or
`scripts/Verify-TerrainStreaming.ps1 -Duration 12` for the bounded automated
route. The automated proxy travels from x=-160 to x=416, z=24, at accelerated
diagnostic speed. It crosses five boundaries and visits six body tiles. The
camera follows that body; it does not select terrain residency.

The host retains a 3x3 neighborhood of 128 m tiles, removing outgoing tile
entities and creating incoming meshes synchronously. It submits source normals
and moisture/sea-derived colors, compares actual float mesh edge heights and
normals, and builds a rendered-triangle cache from the same float vertices and
indices. The proxy's bottom-center height follows that cache. This measurement
does not cover bottom-face corners, feet, slope alignment or animal locomotion.
Post-update scene bounds and capture-time visibility/projection are also
reported. Object and meshlet occlusion culling are explicitly disabled in this
accelerated diagnostic; production culling and frame-rate acceptance are not
established by it.

The verification script checks numeric metric presence/types/finiteness before
thresholds, rejects missing/negative fields and coerced string capture flags,
and requires bounded residency, actual
eviction, crossings, support samples and three saved capture files. Pass
`-Evidence <existing evidence directory>` to verify a receipt without launching
another process. Review captures separately; saved files and counters do not
establish visual quality.

This route has no water surface, habitat props, population, save/load or join to
the existing watershed. It rejects combinations with watershed, asset and
persistence options. The populated watershed still uses its original detailed
terrain. Integrating its coordinate/elevation and boundary policy is required
before this can become the actual populated world's terrain. Native reverse
traversal after eviction, mixed LOD, preparation budgets and target-hardware
frame times remain open.
