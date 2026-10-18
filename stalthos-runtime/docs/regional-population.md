# Retained regional population registry foundation

Status: **Decided domain foundation; native host integration To test/unimplemented.**

`domain/regional_population.hpp` owns retained `Population` instances and their
`ResourceLedger` instances. Mandatory bootstrap takes the existing legacy
population, its complete resource/history snapshot and an explicit reservation.
It preserves supplied individual IDs, habitat signature, route intents, needs,
resource quantities and replay keys. It does not regenerate or rename the legacy
population. No STL8 code or persistence representation changes in this packet.

At most eight regions and 64 actual individuals are retained; each existing
`Population` retains its 16-actor bound. These are actual identities, not coarse
island estimates. Admission only accepts the explicit PhysicalArrival origin.
CameraPrewarm is rejected; activation changes from camera origin are rejected.
The registry has no HeroSim perception API. Production callers remain responsible
for distinguishing physical arrival from camera movement.

Each region reserves resource slots and history keys. Global reservation totals
cannot exceed 4096 resources or 256 history keys, including legacy reservations.
A reservation includes already-used keys: 240 used keys with a reservation of 240
leave 16 global keys, rather than charging 480. The registry imports ledger
snapshots under those limits and checks resource IDs globally. All actor IDs
must be unique across retained regions. Admission rejects capacity, duplicate
region/identity/resource, seed conflicts and invalid snapshots before commit.
It never evicts retained regions, individuals, resources or history keys.
Returned snapshots are detached copies; no mutable population/ledger access exists.
Resource slots reserved beyond imported resources currently remain unused: there
is no resource insertion or reservation resizing API.

Departure toggles active state without dropping snapshots. Active updates call
`Population::advance(dt, false)`; dormant updates use `advanceDormant`. The latter
retains pose/routes and advances needs/age. Ledger consumption is permitted only
for active regions. Per-region ledgers enforce reserved key capacity and retain
replay/update behavior at capacity. They form disjoint global resource namespaces.

`configureContacts` associates existing resource IDs with supplied world anchors
and the population's actual read-only habitat sites. Each resource has one anchor;
each site has at most one anchor per resource kind. Configuration checks finite
positions, site bounds, matching browse/freshwater kind and a fixed 1.25 m 3D
reach from the habitat site. Configuration is immutable. Actual rendered ground
and world-resource anchor provenance remain responsibilities of future native
world construction; the domain adapter cannot verify rendered geometry.

Physical active arrival may bind each living retained actor to one nonzero,
immutable session. Sessions are unique across the registry, a deliberately
conservative policy even though resource namespaces are disjoint. Session binding
is bounded by retained actor count. Any imported history session is permanently
unbound bookkeeping history: the adapter cannot infer its actor provenance and
rejects attempts to claim it. All its original keys and quantities stay retained.

`contact` validates origin, active region, actual living actor, bound session,
actual site, anchor/resource identity, 3D reach and Browse/Drink activity. Only a
ledger NewKey or Update returns a fresh event and grants relief, using the actual
accepted cumulative difference (food × 0.1, water × 0.2, with existing Population
per-call relief caps). Replay, history-full denial, insufficient quantity,
camera/dormant activity, wrong site/resource and missing/dead actors grant no
relief or event. The result carries at most one event; there is no mutable event
queue to replay. Availability derives from ledger quantities and anchors before
each active population advance, so depleted sites stop attracting new activity.
`consumeCumulative` remains explicitly bookkeeping-only and grants no relief.

`RegionalContinuationDto` is a detached in-memory snapshot of all retained
population fields, complete ledger history/quantities, reservations, anchors,
actor/session bindings, configuration flags and active state. Restore requires
the same already-admitted seed, legacy region, region set, actor identities,
habitat signatures, resource IDs/kinds/initial quantities, anchor configuration
and reservations. Immutable habitat topology is supplied by that existing world;
the DTO does not construct a new world or encode a new disk save format.
Resource availability flags are derived from restored quantities and immutable
anchors on the candidate copy before commit. Validation occurs on a full detached
copy and commits by map swap: malformed late regions/keys/bindings leave the live
registry unchanged. Restore rejects imported-key theft, reassigned existing
actor/session ownership, retained bound history missing its actor, cross-region
session aliases, missing/extra regions and reservation/configuration drift.
Fresh equivalent registries can restore valid adapter-owned continuation history.

**Integration limits:** no physical-region boundary/habitat construction,
cross-region movement, persistence archive/version, native host wiring, or native
visual acceptance is implemented. A host must transfer **all** retained legacy
keys into registry ownership; a separate host ledger would escape these budgets.
Legacy contact ownership migration needs an explicit provenance contract before
old unbound keys can become actor-bound. The current populated legacy world and
STL8 behavior are untouched. Native multi-region save/load and real rendered
resource-consequence evidence are still required before claiming population
expansion.

Verification: `tests/regional_population.cpp` runs native C++ domain fixtures for
legacy preservation, disjoint regions, exact departure/return, elapsed dormancy,
rejected camera admission/activation, seed/identity/resource conflict atomicity,
detached snapshots, hard limits and full legacy 256-key replay/update/rejection.
Its `REGIONAL_POPULATION_FOUNDATION_OK` diagnostic explicitly reports
`host_integrated=false`. Domain fixtures do not establish integrated rendering,
full draft completion, foot contact or 30 FPS acceptance.

`tests/regional_contacts.cpp` adds native domain checks for actor-bound food/water
accepted deltas, replay and denial without relief/events, immutable sessions,
imported-history theft prevention, exact multi-region departure/dormancy/return,
restore into equivalent admitted layouts, derived depleted routing and malformed
continuation failure atomicity. `REGIONAL_CONTACTS_OK` explicitly reports
`host_integrated=false`. Neither fixture establishes native rendering or 30 FPS.
