# Relationships Foundation Draft

## Overview

The `relationships.hpp` file provides an engine-independent C++17 foundation for maintaining bounded identity-based relationship records in *The Stalthosian Archipelago*. It is part of the native runtime domain and defines pure query/suggestion state for social ties between entities.

This system tracks specific roles and contact observations between known individuals. It is strictly bounded, stores edges in deterministic identity order, and rejects unbounded graphs.

## Core Concepts

*   **Identities:** Identifies entities via 64-bit integer values (`stalthos::Identity`).
*   **Roles (`Role`):** Shared substrate classifying the type of relationship. Currently supports:
    *   `Peer` (reciprocal bonding)
    *   `Caregiver`
    *   `Dependent`
*   **Contact Status (`ContactStatus`):** The last known status of the relationship from the observer's perspective:
    *   `Present`
    *   `Separated` (temporarily lost contact/absent/unknown)
    *   `ConfirmedDead` (the relationship is permanently terminated by death)

## Design Rules & Constraints

1.  **Pure State, No Logic Controller:** This foundation tracks state; it does not dictate behavioral updates, AI simulation, or act as a second HeroSim controller. The behavior (e.g., bonding, parenting, reproduction) specific to the five playable species remains external to this task.
2.  **Directional Observations:** Present and Separated link states update for the observing individual only. A separation event neither creates an entity nor forces contact-state changes onto another observer.
3.  **Death Truth:** Confirmed death is tracked for the target identity globally within this tracker, while the death observation timestamp/status stays on the observer's link. Later present/separated observations and new relationships involving that identity throw `std::logic_error`. Missing or unknown identities remain distinct from confirmed death.
4.  **Bounded Data & Determinism:** The `RelationshipTracker` strictly limits the number of nodes (tracked entities) and links per node. Limits are enforced via `std::overflow_error`, including a zero-link capacity when both endpoints are new. The confirmed-death ledger contains only identities already present in this bounded graph. Duplicate or self-links are strictly prevented via `std::invalid_argument`. Reciprocal link formation is staged and committed as one graph update so allocation failure cannot leave only one side.
5.  **Time Validity:** Observation events must have non-decreasing, finite, non-negative timestamps. Time zero is valid for initial-world observations.

## Missing Integration

This is a foundation draft. The following features require future integration by the root worker:
*   **Host and AI Integration:** Needs to be wired into the engine host update loop to actively record sightings, separations, and deaths using sensor data.
*   **Persistence Integration:** Save and load logic is not implemented here. Bounded capacities ease future serialization without dynamic sizing issues.
*   **Mapping to Foundation Retentions:** The future root integration can utilize relationship links to assign `RetentionReason` values (such as `NotableEncounter` or `BondedCompanion`) within `foundation.hpp` without modifying the core domain contracts established here.

## Provisional Tuning

The default configuration of `RelationshipTracker` allows 1,000 active nodes and a maximum of 10 tracked links per node. This is a provisional limit intended to support small local family/peer groups without modeling a persistent global social network. Tuning values should be finalized after representative species (such as Brackenback and Reedcrest) are behaviorally evaluated in the full draft.
