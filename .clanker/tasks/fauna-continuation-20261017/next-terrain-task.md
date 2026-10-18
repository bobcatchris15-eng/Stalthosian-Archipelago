# Next bounded task: coarse terrain tile sampling and stitching

Read HALT, current-work and the fauna-continuation receipt first. No old Jules retrieval. No HeroViewer or mesh polish.

Objective: establish a tested tile contract for explorable coarse geography, using `domain/archipelago_world.hpp` elevation/moisture as the source field. Before dispatch inspect existing isolated tasks for terrain ownership. Use a new isolated worktree, at most one worker for this contract.

Proposed ownership: new `domain/terrain_tiles.hpp`, `tests/terrain_tiles.cpp`, and `docs/terrain-tiles.md`. Root alone wires CMake and engine presentation. Preserve `domain/reference_landscape.hpp`, `engine/terrain_surface.hpp`, population persistence and all planning inputs.

Acceptance: bounded tile index/resolution inputs; deterministic global vertex coordinates independent of tile visitation order; identical shared-edge heights and finite-difference normals including negative tile indices; moisture/sea mask derived from the same sample; bounded neighborhood selection around animal body; explicit overflow/NaN/bounds failures. No camera-driven physical activation. No remote population count presented as individual simulation.

Then root integrates a reversible rendered neighborhood into Wicked, verifies actual boundary traversal/contact and captures native evidence. Domain-only passing tests do not establish streamed playability, authored coasts, final visuals or frame-rate acceptance. Preserve existing 192m watershed until its join policy is explicit and measured.
