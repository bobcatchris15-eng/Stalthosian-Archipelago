# Native domain foundation

`foundation.hpp` is a standard-library-only C++17 header in namespace `stalthos`.
It establishes a few executable correctness contracts for the full draft. It
does not implement the whole game, navigation, a renderer, species biology or
disk persistence. Its state survives region snapshots and hero replacement
**within one process**. Closing the process loses this state.

## Host interface

Coordinates use metres, Y up, and heading zero toward +Z. Angles are radians.
IDs are caller-assigned unsigned 64-bit values. The host must allocate unique
identities and classify genuinely bodily stimuli after sensory filtering.

- `SimCore::advance(physicalSeconds, calendarScale)` returns a `TimeStep` with
  physical and calendar deltas and advances the single calendar. Feed physical
  seconds to contact/input/motion; feed calendar seconds to coarse age, season
  and condition advancement. Rest acceleration does not multiply contact input
  or charge movement twice. Scale zero pauses the calendar. No scheduler or
  hazard-sampled time skipping is implemented yet. Invalid/nonfinite/overflowing
  time requests throw without advancing the clock.
- `BeastCore::bodyForward(bodyHeadingRadians)` yields the unit forward vector.
  The API has no camera parameter. This is direction calculation, not a gait,
  turn-radius or physical locomotion implementation.
- `HeroSim::suggest(target, tolerance, contextual)` creates one voluntary goal.
  `authority()` chooses Direct, DestinationOrContext, or Autonomy.
  `setDirect(true)` cancels outstanding goals; passing `preserveGoal=true` is the
  explicit preservation exception. Releasing input restores autonomy unless a
  preserved goal remains. Paused and replanning goals remain outstanding.
  `setGoalStatus` completes, abandons or cancels them. Routine hunger cannot
  replace movement authority. Actual feeding, navigation, reflexes and
  autonomous goal selection remain future work.
- `HeroSim::perception.observe(BodyStimulus)` accepts bodily observations.
  `HeroView::cameraAttention` holds a distinct `CameraStimulus` type that cannot
  be passed to this API. Both listeners can refer to one sound source ID. The
  host must never relabel camera-only data as a bodily stimulus. Current
  attention holds one latest observation; occlusion, hearing sensitivity,
  attention expiry, camera tether/framing and alarm recall are not implemented.
- `StalthosState(seed, retentionLimit)` reconstructs stable region seeds without
  implementation-dependent `std::hash`. `recordTree` and `regionSnapshot`
  preserve conspicuous tree overrides independently of active render objects.
  Snapshots expose a revision; `snapshotCurrent` rejects preparation based on
  an older region revision. The host must use this check before committing
  asynchronous results. Geography and seasonal reconstruction remain pending.
- Stage an animal with `instantiate(id, region)` before `retain(id, species,
  reason)`. Retention is only available for instantiated animals, is capacity
  bounded, and never silently evicts a known identity. `retire` removes its
  active instance but keeps eligible compact identity; `reactivate` creates
  exactly one active representation and refuses dead identities. Dead retained
  animals continue occupying a slot until a future explicit identity-decay
  policy is added. Ordinary transients retire without a biography. The active
  actor population is bounded by the future activation/streaming host, not by
  this retained-identity cap. Do not call `retire` on the current hero: its
  lifecycle transition is `heroDied`.
- `setHero` reserves the new hero's identity from local population staging;
  `heroDied` preserves its conspicuous death and clears the current hero without
  resetting regions or companions. Hero death exception records are distinct
  from bounded retained fauna; their eventual carcass/identity decay and save
  limits are future work. No identity is generated for remote wildlife. The
  five species enum values are profile identifiers, not separate simulators.
- `InteractionsCore::beginSession` opens a monotonically increasing contact
  session. `applyContact` validates contact and available resource, consumes
  each event token once, and rejects stale sessions. The session token set is
  bounded; a full set fails closed. A new contact session clears it, so the host
  must advance sessions only after old callbacks can be identified as stale.
  This simple depletion fixture is not a complete damage/food transfer engine.

The host owns sequencing and all mutable access is single-threaded. No mutexes,
event bus, periodic world organism loop or hidden ecological simulation is
introduced. `StalthosSim`, `FloraSim`, `FaunaSim` and the remaining full-draft
behavior live beyond this packet; the contracts above support their later
integration.

## Verification

Build `tests/scenarios.cpp` as a standalone console program. `tests/CMakeLists.txt`
also provides a standalone CMake/CTest project; the root engine build can add
the executable directly if preferred.

Verified on the local VS 18 BuildTools MSVC toolchain:

```text
cl /nologo /std:c++17 /EHsc /W4 /WX /permissive- /Fo<build>/scenarios.obj /Fe<build>/scenarios.exe stalthos-runtime/tests/scenarios.cpp
scenarios.exe
6/6 scenarios passed
```

The six scenario groups test calendar acceleration/dilation and responsive
physical deltas; forward movement at four body and four camera headings;
goal priority/pause/resume/completion/cancellation; compile-time rejection of
camera stimulus injection; invalid, duplicate and stale resource transfers;
and seed, fallen tree, bounded retention, duplicate exclusion, dead companion
and repeated hero death continuity. These are no-GPU contract tests and provide
no evidence of rendered quality or frame time. The separate persistence DTO
checks and its scope are described below.

## Persistence DTO boundary

`persistence.hpp` provides a versioned, engine-independent `SaveData` format
(`STL1`) and a checked reconstruction seam for the fields represented in that
DTO: calendar time, world seed, retention limit, entity IDs and regions,
retained-entity profile/status, dead-hero identity, and tree exceptions. It is
not wired to `engine/main.cpp` or a host snapshot source, and the DTO omits
transforms, body state, needs, age, and other game state. The running game does
not yet provide player save/load. Do not describe this seam as complete game
persistence.

The reader checks the whole file byte limit before parsing, then bounds each
collection and aggregate tree count before reserving storage. Both read and
write reject invalid time values, enum values, retained counts above the
retention limit, duplicate entity identities across lists, repeated regions or duplicate tree IDs within one region, and dead retained records marked active. Writes validate before
touching the prior save, use a temporary sibling file, remove it after failures,
and rename it into place only after a successful flush and close. Records are
serialized in stable ID/region/tree order. The standalone Windows tests cover
malformed inputs, bounded file size, duplicate IDs, deterministic bytes,
preservation of a prior save on write failure, and checked domain restoration.

Host integration still needs an authoritative entity/catalog snapshot and an
apply path. The current `StalthosState` API does not expose its retained, active,
dead-hero, or region inventories; `recordTree` increments region revisions but
there is no API to restore a prior revision. A narrow root-owned API should
export a coherent domain snapshot and apply it atomically, including explicit
region revision restoration or a documented cache invalidation rule. The host
must validate entity and tree references against its actual catalog; the DTO
can only reject internal duplicates and conflicting IDs because that catalog
is not available here.

Format version 1 fixes the enum ordinals and collection bounds. Changing those
fields requires an explicit compatibility/versioning decision.
