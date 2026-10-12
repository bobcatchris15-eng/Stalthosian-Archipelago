# Activation Plan Draft

## Scope
This document covers the bounded region activation planner. It separates near-body physical staging, camera-visible presentation staging, and distant approximate or dormant regions. Bounded budgets and hysteresis limit churn. Inputs contain stable region IDs, positions, current tiers, separate body and camera locations, and tunable radii and budgets. Outputs are sparse plans: each listed region receives a physical or presentation tier; omitted IDs are assigned distant, so consumers must downgrade any prior physical or presentation staging for those regions.

The planner accepts at most 100,000 regions, with each tier budget capped at the same limit. Non-finite inputs are rejected, as are finite inputs whose derived distances or hysteresis limits overflow. A plan never creates population, rendering, streaming, a body stimulus, or a change to hero perception.

## Missing Integration
- **Source population reconstruction/catalog:** Actual streaming and population instantiations are not implemented. The output of the planner is just a region tier map, which must be executed by another system.
- **Actual Wicked streaming:** The planner does not directly invoke or control Wicked Engine streaming. It provides the bounded region data for the engine adapters to execute on.
- **Measured performance acceptance:** Tuning parameters (radii, hysteresis, budgets) are provisional. Full release tuning and performance measurements are incomplete. This needs real frame-time trace metrics per the full-draft acceptance criteria once integrated with Wicked Engine.
- **Host and persistence integration:** Storing and restoring the state of these tiers during saves and bounds checking against `StalthosState`'s stored regions is not yet wired up.
- **Incomplete full-draft scope:** This planner only provides the region tier planning piece. Animal behavior and other domains are not instantiated by this planner. Camera presentation cannot create `BodyStimulus` or change `HeroSim` perception.
