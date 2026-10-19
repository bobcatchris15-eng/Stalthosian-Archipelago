# Connected world working draft

Run from the repository root:

```powershell
.\stalthos-runtime\scripts\Run.ps1 -ConnectedWorld -Interactive -CameraDistance 10
```

This selects the normal populated host with an exterior route variant. W/S move
the body, A/D turn, and the existing mouse camera controls still apply. The body
starts on the dry bank at (24,24), facing east toward the saddle. The original
watershed terrain and original eight retained animals remain in this host. The
connected preview explicitly refuses load/save; use the original PopulatedWorld
mode for existing saves. It creates no exterior retained populations.

The route is real rendered ground: the existing streamed exterior meshes sample
ConnectedRoute, their normals use the same surface, and body/camera support uses
the resident triangles. The original192m reference mesh is unchanged. The former
eastward climb to84m is lowered to a gentle open lowland around10m, with a broad
lateral blend into remaining bluffs. Dry grasses and five organic rock clusters
make the saddle and sandy meadow distinct from the creekbank. Terrain colors
match the original edge and transition into a drier mosaic over56m, using the
existing terrain surface texture. No source assets were modified.

`domain/connected_route.hpp` owns ConnectedRouteLayout: dry-bank start, route
centerline, fixed reference join at96m, meadow destination, open width, bluff
blend width and gentle rise. Spawn, proof destination/captures, grass trail and
landmark anchors use that layout. This is a first refinable terrain profile,
not an archipelago geography solution.

A bounded proof uses exactly the ordinary2m/s body integration branch:

```powershell
.\stalthos-runtime\scripts\Run.ps1 -ConnectedRouteWalk -Duration 95 -CameraDistance 10
```

It injects forward intent and does not assign scripted body coordinates. Outputs
include per-step dt/speed/displacement/ground trajectory, ground-support metrics,
and player-view captures at approximatelyx72,110,170,200. The earlier
PopulatedTraversal teleport probe remains separate and cannot be combined.

Working-draft oddities to carry forward, rather than polish now:

- The lowered band continues east past the tested destination; no authored end
  blend or complete island boundary is claimed. Far flanks remain steep coarse
  terrain. Other directions can still look like an enclosed fixture.
- Grass and landmarks are a bounded always-resident composition for this route;
  the full landscape content streaming/population system is unfinished. Grass
  density and rock clustering are provisional, and some rock bases may intersect
  slopes. Placement uses the same deterministic2m triangle interpolant.
- The hero and watershed animals are still engineering proxies. No creature
  anatomy, final art, interaction contact or target-hardware FPS acceptance.
- The camera keeps its existing global-orbit behavior after turns. This variant
  initializes the view toward the passage; it does not redesign camera controls.
- No new resource interaction or exterior ecology is added. The next subsystem
  should add visible world life/reactions along the working route.
