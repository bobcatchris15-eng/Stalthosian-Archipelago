# Populated world terrain residency

`Run.ps1 -PopulatedWorld` now retains the original 192m reference terrain and adds joined exterior terrain around the body. Existing local positions, creek geometry, resources, habitat sites and population save signatures remain unchanged. A new world is not required for this terrain presentation change; persisted inactive populations still retain the previous frozen behavior.

`engine/PopulatedTerrain` uses the immutable `WatershedTerrain` sampler with 128m tiles and 2m cells. Rectangular patches split at the original +/-96m edges and omit cells inside the original square. Exterior caches read engine mesh positions/indices; contact queries prefer the retained original mesh, then resident exterior rectangles. Shared original-edge shading normals are copied from the original renderer mesh. Texture/material continuity is not established by normal agreement.

Body residency requests a radius-one tile neighborhood and always retains the four tiles enclosing the original watershed. Eviction removes only owned exterior entities. Camera queries consume this residency but do not activate populations. The local eight individuals continue simulating while the body travels away; regional habitat populations, body activation, retention and coarse dormant evolution remain later work. Follow-camera bounds and populated movement/load bounds extend beyond the reference patch, with body coordinates bounded to +/-12000m inside the sampler bounds. Focused hero diagnostics retain their previous behavior.

For a bounded accelerated traversal through the actual populated update/resource/save loop:

```powershell
./stalthos-runtime/scripts/Verify-PopulatedStreaming.ps1 -Duration 24
./stalthos-runtime/scripts/Verify-PopulationContinuation.ps1 -FirstTraversal -FirstDuration 24 -SecondDuration 4
```

The traversal travels (24,24) to (544,24), holds the outbound endpoint, and returns. Diagnostic-only occlusion disabling makes the fast marker route observable; normal gameplay keeps its existing culling. `populated-terrain.json` records tile reconstruction, actual shared mesh vertices/normals, rendered triangle support, root error and spatial capture locations. Inspect all four captures independently; a passing receipt cannot establish visual quality. The brown hero marker's bottom has a preexisting approximately0.25m clearance before slope tilt; root grounding and marker-bottom clearance are deliberately separate metrics.

Current limits: synchronous tile construction, finite visible terrain edge, sparse coarse exterior, steep inherited blend slopes and abrupt material transition. Normal imported-creature sole diagnostics remain restricted to the original landscape; terrain body support is approximate and does not establish per-foot contact. Neither this engineering traversal nor a passing build accepts final scenery, full-draft completion or the target 30FPS floor.
