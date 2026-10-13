# Woodland Stands Placement Strategy

This document outlines the proposed design for deterministic background tree stands in the Archipelago.

## Implementation

The `WoodlandStands` class provides a bounded, deterministic distribution of tree assets across valid dry terrain.
*   **Asset Metrics:** Utilizes "Splitcrown" tree bounding dimensions. Default young adults stand 14m high with a 6.5m crown. A secondary sapling stage stands 4.2m high with a 2.4m crown. Note: whole-tree actual dimensions are not claimed here. Splitcrown asset audit currently produces 26m trunk height regardless of stage; Jules 8336722969998514867 canonical stage fix is pending. Proposal 14m adult / 4.2m sapling represents authored intended metrics only.
*   **Placement Bounds:** Trees are strictly confined to the background valley (`z` roughly `[-65, -12]`) on both dry banks (`x <= -15` or `x >= 20`) within the `192m` reference landscape patch.
*   **Derivative Probe Bounds:** Derivative probe queries for numerical slope estimation (`x + 0.1`, `z + 0.1`) explicitly validate landscape bounds prior to sampling `landscape.height(...)`, preventing bounds violations for valid centers near the patch edge. Finite mathematical checks protect slope denominators and height computations.
*   **Irregular Clusters & Bounded Attempts:** Trees are distributed in small clustered stands with bounded exploration: 50 cluster attempts with up to 15 tree candidate attempts per cluster (750 total candidate attempts ceiling), supporting up to 32 trees max (default cap 20).
*   **Truthful Budget Reporting:** Incomplete tree quotas are truthfully exposed via `candidateAttempts()`, `clusterAttempts()`, and `isCandidateBudgetExhausted()`. Stands never falsely claim that 20 trees were always placed if candidate exploration was exhausted by tight landscape constraints.
*   **Prefix Stability:** Identical seed sequences guarantee prefix-stable tree placements across budgets (e.g. running budget 1, 20, or 32 yields identical placements for elements `0..N-1`).
*   **Validity:** The generation algorithm strictly respects a central 8m radius inspection corridor around the hero position (12, 0) and the creek wet mask. Placements are evaluated against steep terrain slopes (rejected if `> 20` degrees) and guaranteed to be mathematically finite.
*   **Determinism:** Stable IDs, headings, and scales are calculated completely using deterministic hash mixtures based on seed, rather than global random state or unseeded hashes.
*   **Fail-Safe:** Impossible placement constraints lead to bounded termination without hanging, rejecting oversize constraints typed with explicit `std::invalid_argument` for requests exceeding the hard cap of 32 trees.

## Actual Tests

Offline verification of this underlying domain structure guarantees correctness via `stalthos-runtime/tests/woodland_stands.cpp`.
The tests execute without a physics/ecology claim, or rendering dependency:
*   **Repeatability:** Evaluating a stand with the same seed results in an identical collection of tree properties. Different seeds yield different distributions.
*   **Budget & Prefix Stability:** Ensures correct handling of limits `0`, `1`, `5`, `12`, `20`, and `32`. Confirms identical prefix placements across budgets for identical seeds.
*   **Hard Cap Rejection:** Verifies typed `std::invalid_argument` rejection for hard cap violations (`> 32`).
*   **Multi-Seed Property Scans:** Actively scans 100 seeds verifying finite coordinates/headings/scales/props, unique placement IDs, valid dry bank extents (`x <= -15` or `x >= 20`), valley bounds (`z` in `[-65, -12]`), 8m hero corridor clearance, creek wet mask exclusion, surface height alignment, and truthful exhaustion flags.
*   **Release-Active Assertions:** Uses `require` logic actively valid for `MSVC/Release` assertions (bypassing `NDEBUG` omissions).

### Real Test Execution Output

Direct standalone compilation with MSVC 18.6 (`Visual Studio 2026 Developer Command Prompt v18.6.0`, `/std:c++17 /O2 /W4 /WX /EHsc /permissive-`):

```text
Starting WoodlandStands regression and property verification...
PASS: Determinism and seed divergence checks.
PASS: Budget prefixes (0, 1, 5, 12, 20, 32) stability checks.
PASS: Hard cap (>32) typed rejection checks.
PASS: 100-seed property scan (finite, dry bank, slope, corridor, unique IDs, truthful exhaustion).
All woodland stands regression and property tests passed successfully.
```

## Pending Validation

*   **Runtime Integration Missing:** The parent host engine integration connecting the domain structure output to the rendered scene has not yet been implemented. The parent will register CMake dependencies and import actual tree instances subsequently.
*   **Full Draft Acceptance:** Frame stability (30 FPS budget) and artistic acceptance cannot be proven until the trees render.
*   **Rendering Specifics:** The explicit Y analytical geometry in the output array provides a placement baseline. The renderer's actual triangle base sampling remains pending to avoid geometric ground gap anomalies.
*   **Asset Pipeline & Stage Fix:** Whole-tree actual dimensions are not claimed. Splitcrown asset audit currently produces 26m trunk height regardless of stages; Jules 8336722969998514867 canonical stage fix is pending. Proposal metrics (14m adult / 4.2m sapling) reflect authored intended bounding values.
