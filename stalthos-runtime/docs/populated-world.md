# Populated world construction checkpoint

The world construction path is `scripts/Run-PopulatedWorld.ps1`. It starts the
Wicked runtime with eight autonomous animals on the detailed reference watershed.
Available local creature, fern and tree exports are reused. Missing creature
exports use simple geometric proxies; population work does not require final art.
Run with `-Interactive` to explore, or leave the default bounded thirty-second run
to collect screenshots and JSON under `var/reports/runtime/`.

Each animal owns its identity, fictional species profile, age, needs, activity and
route progress. The camera does not create, remove or choose behavior for animals.
Actors follow a connected graph of dry routes built from the actual terrain mesh,
water mask and shrub positions. Shared lifecycle and needs components use the
world clock. Browse and drink contacts consume the same resource ledger the hero
uses, and shrub presentation reads that shared depletion. Continuous consumption
uses one cumulative replay record per actor/resource rather than one per frame.

`world-population.json` records initial/final actor state, resource consumption,
activity frames and grounding updates. `world-regions.json` records sampled
conditions for Main Island, Rainridge, Stone Island and Outer Reach. The coarse
archipelago supplies actual elevation/moisture queries across a 28 km marine
extent. Regional weather uses the shared calendar and drives Wicked wind/rain.
Regional population numbers are seeded capacity estimates; remote individuals
are not simulated yet. Weather, consumption and behavior rates are provisional.

Important construction limits:

- The rendered detailed patch remains 192 m across. Coarse terrain is not yet
  stitched to it or streamed into explorable islands.
- Active population identities/state are not saved yet. Resource depletion uses
  the existing persisted ledger, so a saved world retains consumed resources.
- Species currently share the available creature mesh. Proxy animals have no
  authored gait. Neither mode establishes final anatomy or animation acceptance.
- Coarse island masses still read rounded; detailed coasts, connected drainage,
  regional plant communities and population activation/retention remain work.
- Continuous moisture and descriptive region metadata differ, especially for
  Stone Island. The conditions sampler deliberately consumes the actual field.
- No target-hardware frame-time or 30 FPS acceptance is claimed.

The next construction work is terrain streaming and regional habitat population,
population continuation/activation, and observable environmental consequences.
HeroViewer and individual mesh polish remain replaceable supporting work.
