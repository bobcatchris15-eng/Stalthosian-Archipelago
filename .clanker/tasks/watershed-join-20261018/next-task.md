# Next bounded work: adopt joined terrain in the actual populated world

Read HALT, current-work and this receipt first. No old Jules retrieval or individual mesh polish.

The world::WatershedTerrain sampler is validated with exact local heights inside96m and exact coarse height/normal at256m. The optional --watershed-join-probe pins four128m central tiles at2m spacing, then travels24->544->24, evicts/regenerates outer tiles and measures actual float mesh support. It is NOT connected to normal populated-world gameplay. Do not repeat this diagnostic as production progress.

Root owns main.cpp/CMake/renderer integration. Extract reusable streamed-surface residency from the probe, retaining original central 2m terrain, water surfaces, local habitats/resources and saved local poses exactly. A production exterior ring must avoid overlapping the retained original192m terrain (clip aligned2m cells at96m or replace with proved identical triangles and preserve ground-cache queries). Resolve FaunaScene::cacheGround single-mesh lookup and current ReferenceLandscape-only camera/ground query bounds explicitly. Mesh positions, shading normals and renderer contact must agree at the original patch boundary. Keep analytical heights separate from actual triangle support, since original noise is discontinuous.

Then connect body-driven terrain residency to actual populated traversal; camera prewarm must remain separate from physical population activation. Keep existing eight watershed individuals and resource history while traveling outbound/return, and verify exact saved identity/pose/route/needs continuity. Add regional habitat placement through bounded deterministic ownership after production ground queries exist. Saved inactive actors remain frozen; activation/retention/coarse dormant evolution are the next priorities. Coarse region counts are not individually simulated animals. Retain the explicit256 actor/resource history-key limit.

Acceptance: native populated outbound/return evicts/regenerates outer terrain; original creek/resource positions and local actors retained; actual rendered triangle contact and boundary normals measured; before/at/after captures inspected. Run Verify-WatershedJoin, existing coarse verifier and Verify-PopulationContinuation after integration. Existing saves remain STL1-STL8 readable. No final-art or target30FPS claim from this engineering probe.

No outstanding workers remain after this task. Keep existing worktrees/evidence/unrelated untracked paths and planning clone untouched.
