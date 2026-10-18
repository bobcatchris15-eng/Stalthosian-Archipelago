# Fauna population snapshots

`fauna::Population::snapshot()` captures the deterministic continuation state: world seed, region, a signature of ordered habitat positions and neighbor topology, and each actor's identity, species, exact position and route indices, heading, distance, lifecycle stage and age, needs, resting flag, and activity.

Restore into a `Population` constructed with the same seed, region, and caller supplied habitat sites by calling `restore(snapshot, excludedIds)`. It builds replacement actors and validates all state before swapping, so a failed restore leaves the current population intact. Seed, region, or habitat signature mismatch fails closed. Actor positions must agree with their current site or lie on the current first-hop route segment. Pass hero, retained, active, and dead identities in `excludedIds` to enforce cross-domain identity ownership.

`persistence::SaveData::faunaPopulation` is optional. New saves use STL7, with a bounded 16 actor payload and strict identity, enum, lifecycle, needs, and route-index validation. STL1 through STL6 remain readable; saves without the optional population load unchanged.
