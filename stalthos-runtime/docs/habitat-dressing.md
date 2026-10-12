# Habitat Dressing Proposals

## Overview

`HabitatDressing` proposes deterministic, bounded reference-creek habitat set-dressing props across the 192 m reference patch (`ReferenceLandscape`). It translates authored ecological and geomorphological zones into believable, clustered prop placements without regular lattice grids.

## Proposed Dressing Kinds

1. **`FallenLog`**:
   - Placed in `Woodland`, dry `Riparian` banks, and `OpenLowland` transitions.
   - **Exclusion**: Strictly excludes wetted channel coordinates (`!waterContains(x, z)`).
   - Clustered along creek banks and woodland margins with randomized orientation and slight terrain-following pitch/roll.

2. **`RiverPebble`**:
   - Clustered along `Creek` bed gravel bars and low riparian water edges.
   - May be submerged or placed on wetted channel gravel deposits.

3. **`DryBankClump`**:
   - Scrub, sedge, or bank debris clumps clustered along the elevated riparian banks and immediate floodplain.
   - **Exclusion**: Strictly excludes wetted channel coordinates (`!waterContains(x, z)`).

4. **`BluffAccent`**:
   - Rock scree, talus, and exposed rock accents clustered around high-relief `Bluff` and `Outcrop` zones on the western portion of the reference patch.
   - **Exclusion**: Strictly dry ground (`!waterContains(x, z)`).

## Algorithmic Contract & Constraints

- **Deterministic Seeds**: All pseudo-random sampling uses 64-bit seed mixing (`mix` / `unit`) derived from `seed_` and cluster indices. Identical seeds and landscape inputs produce bit-identical proposals.
- **Fixed Budgets**: Configurable via `HabitatDressingBudget` (default: 32 logs, 64 pebbles, 48 bank clumps, 32 bluff accents). Placement loops strictly enforce per-kind thresholds, and storage reservation stays capped by the fixed 322-candidate loop bound even for extreme caller budgets.
- **Stable Identifiers**: Each proposal receives a deterministic stable ID (`log_N`, `pebble_N`, `bank_clump_N`, `bluff_accent_N`).
- **No In-Engine Mutations**: Proposes placement descriptors only (`DressingProposal`). Does not delete or mutate animal population, physics objects, or actor entities.
- **Finite Bounds**: All proposals are checked against `landscape.inBounds(x, z)`. Coordinate queries out of the 192 m patch are rejected.

## Status & Missing Integrations

- **Provisional Placements**: Proposal counts and spatial cluster distributions are provisional drafts for offline testing and visualization.
- **Missing Render Wiring**: No mesh bindings, GLTF models, material shaders, or Wicked Engine scene graph insertions are performed. Root integration wires CMake and visual representation.
- **No Scenery Acceptance Claim**: This draft does not claim final visual scenery acceptance or 30 FPS acceptance.
