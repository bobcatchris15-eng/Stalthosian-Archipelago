# Lifecycle Draft Rules

This document outlines the proposed full draft lifecycle rules for the 5 Hero Beasts.

## Hero Profiles (Fiction-First)

The durations for each life stage are proposed draft defaults and are explicitly fiction-first. They do not claim to represent real paleontology.
The stages are:
- Hatchling
- Juvenile
- Adult
- Old
- Dead

Current proposed durations (in calendar seconds) per species:
- **Smalltrotter:** Hatchling: 1000, Juvenile: 2000, Adult: 10000, Old: 3000
- **Brackenback:** Hatchling: 1500, Juvenile: 3000, Adult: 15000, Old: 4000
- **Reedcrest:** Hatchling: 1200, Juvenile: 2500, Adult: 12000, Old: 3500
- **Thistletail:** Hatchling: 800, Juvenile: 1500, Adult: 8000, Old: 2000
- **Longstride:** Hatchling: 2000, Juvenile: 4000, Adult: 20000, Old: 5000

## Progression and Calendar Time

Progression is managed by `TimeStep`, composed of `calendarSeconds` and `physicalSeconds`.
Calendar time determines progression through stages. Physical elapsed time is explicitly separate, so fast-forwarding the calendar does not distort physics interactions.

- A single large delta will yield all crossed stage transitions exactly once.
- Non-finite or negative physical or calendar deltas are rejected immediately.
- A delta that causes an overflow of calendar age is rejected without changing the lifecycle state.

## Terminal Death

Mortality is terminal.
Once the `Dead` stage is reached, subsequent updates cannot resurrect the entity or repeat the death transition. Transitions returned during advancement when already dead will be empty.

## State Restoration

State can be restored using the `restoreState` function.
Restoration performs a strict consistency check to ensure the restored profile, age, and stage form a valid combination. Any invalid configuration will trigger a rejection without modifying the existing state, preserving strong no-change-on-error semantics.

## Explicitly Unfinished Features

The following functionalities are explicitly unfinished for the draft and will throw an error if called:
- Reproduction
- Parenting
- Update Needs

The implementation avoids fabricating completion or inventing unsupported mechanics for these systems.
