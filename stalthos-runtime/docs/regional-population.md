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

**Integration limits:** this is domain ownership and admission infrastructure.
`consumeCumulative` only updates the ledger; it does not bind a session to an
actor, grant accepted-delta needs relief, synchronize edible resource site
availability, or validate reach. No physical-region boundary/habitat construction,
cross-region movement, persistence archive/version, native host wiring, or native
visual acceptance is implemented. A host must transfer **all** retained legacy
keys into registry ownership; a separate host ledger would escape these budgets.
Before native integration add actor-bound resource presentation/contact and
accepted-delta relief with no replay/denial benefit, plus exact multi-region
save/load. The current populated legacy world and STL8 behavior are untouched.

Verification: `tests/regional_population.cpp` runs native C++ domain fixtures for
legacy preservation, disjoint regions, exact departure/return, elapsed dormancy,
rejected camera admission/activation, seed/identity/resource conflict atomicity,
detached snapshots, hard limits and full legacy 256-key replay/update/rejection.
Its `REGIONAL_POPULATION_FOUNDATION_OK` diagnostic explicitly reports
`host_integrated=false`. Domain fixtures do not establish integrated rendering,
full draft completion, foot contact or 30 FPS acceptance.
