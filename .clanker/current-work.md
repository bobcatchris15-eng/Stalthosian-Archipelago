# Current world construction checkpoint — 2026-10-17

Direct user priority: world, simulations, systems and population. Meshes/assets
are best effort and replaceable. HeroViewer/Smalltrotter polish is off the critical
path unless it blocks world construction. Root coordinates and implements details;
use light capable workers with bounded ownership and independently verified output.

## Validated now

Eight autonomous active-region animals run on actual terrain-derived dry habitat
routes, sharing existing lifecycle/needs components and clock. Browse/drink use
the hero resource ledger and shrub depletion presentation. Draft creature imports
are optional; a proxy-only save/load runtime also passed. Final spawn fix distributes
browsers/drinkers across available sites. No camera-driven simulation activation.

Actual30s run:8 actors,4 moving,4 activity categories,0.778708 food and0.519139 water
consumed,9580 contacts,26 condition updates. Save/load creek continuation error0.
Final10s separated spawn replay also consumed both resource types. NativeRelease
build and28/28 CTest passed. Toolkit earlier same-session2073pass/2skip; no further
toolkit changes in world packet. Exact evidence tasks/world-population-20261017/receipt.json.

Integrated Jules regional weather and AGY coarse archipelago: Main Island,
Rainridge, Stone Island, Outer Reach. Actual64x64 atlas flood fill found4 distinct
land components; plot under var/reports/world-population-20261017/world-atlas.png.
Regional conditions consume actual continuous moisture and drive Wicked rain/wind.
Coarse regional counts are seeded capacity estimates, NOT thousands of simulated
animals. Coarse islands remain rounded and moisture differs from descriptive
metadata (especially Stone); preserve these limitations.

## Next world work

1. Stream/stitch coarse terrain into explorable regions with habitat patches,
   authored coasts/drainage and regional vegetation communities.
2. Persist population state and implement activation/retention/coarse cohorts.
3. Add observable interactions and environmental consequences; improve collision/
   resource crowding and replace bounded resource history with production epochs.

Detailed rendered patch remains192m; no full four-island playability, final
visual/gait acceptance, or target-hardware30FPS claim. Run with
stalthos-runtime/scripts/Run-PopulatedWorld.ps1 (-Interactive optional).

## Backlog and preservation

Jules31-session audit collected26completed,4awaitingfeedback,1failed/30patches.
Three fixes integrated;3 rejected as written;24 deferred;1 no-diff already handled.
New Jules3306157609621882461 completed/reviewed/integrated. Do not refetch old patches.
AGY archipelago and native fauna/environment reviews completed; no active workers.
Earlier seam/needle/crossfade checkpoint published86ade13. Artwork refinement remains
provisional and deferred. See tasks/continuation-20261017/receipt.json.

Rauken-Archipelago stays read-only. Preserve unrelated untracked logs/receipts/objs.
Validated origin/main publication authorized; no force/release/deployment/paid API.
Check .clanker/HALT before dispatch. Full draft remains incomplete.
