# Fauna population snapshots

`fauna::Population::snapshot()` captures the deterministic continuation state: world seed, region, a signature of ordered habitat positions and neighbor topology, and each actor's identity, species, exact position and route indices, heading, distance, lifecycle stage and age, needs, resting flag, and activity.

Restore into a `Population` constructed with the same seed, region, and caller supplied habitat sites by calling `restore(snapshot, excludedIds)`. It builds replacement actors and validates all state before swapping, so a failed restore leaves the current population intact. Seed, region, or habitat signature mismatch fails closed. Actor positions must agree with their current site or lie on the current first-hop route segment. Pass hero, retained, active, and dead identities in `excludedIds` to enforce cross-domain identity ownership.

`persistence::SaveData::faunaPopulation` is optional. New saves use STL8, with a bounded 16 actor payload and strict identity, enum, lifecycle, needs, and route-index validation. STL1 through STL7 remain readable; saves without the optional population load unchanged.

STL8 adds stable actor-to-resource session mappings and restores cumulative ledger
contacts across repeated loads. Original STL7 population payloads remain readable
without mappings. Dead animals retain their exact position along a habitat route.


`advanceDormant(TimeStep)` performs cheap elapsed lifecycle and needs evolution on
copies, committing only after every actor validates. Both time components retain
their usual meanings: calendar time advances age/hunger/thirst, physical time
advances fatigue using the stored resting flag. It does not move bodies, choose
activities or routes, browse, drink, query resources, or create resource contacts.
Living actors retain their exact activity, route, pose, identity, injury and
resting flag so mid-route snapshots remain valid. The host retains resource
session mappings/history separately; dormancy never touches them. Zero elapsed
time is a no-op; invalid time and arithmetic overflow leave all actors unchanged.

Death follows existing active-step semantics: transition to Rest, retain corpse
position/route and last needs. Needs are not advanced on a step ending dead, and
already dead ages remain frozen. Split and aggregate updates therefore agree
within floating point rounding for living actors away from death transitions;
partition-independent death-step needs are not promised. No save format changes
are required. This primitive does not itself implement host activation policy.
