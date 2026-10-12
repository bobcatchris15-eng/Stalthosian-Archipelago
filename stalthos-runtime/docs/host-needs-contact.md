# Host Needs and Physical Contacts

This is a provisional host interaction draft for the current hero. The body transform and finite reference landscape determine contact; camera position does not. In normal play, the player must press E. Needs suggestions do not steer or move the hero.

## Draft tuning

All five fictional hero forms currently share provisional rates: hunger rises by `0.0001` and thirst by `0.0002` per calendar second; fatigue rises by `0.005` per physical second and recovers by `0.05` per physical second while resting. These values are functional placeholders, not biological claims.

Drinking is eligible when the living hero's body root is within 3 m horizontally of the creek center, stands over a wetted fixture point, and is no more than 1 m above its water surface. Browsing is eligible within 2 m horizontally and 1.5 m vertically of one of the eight fixture shrubs that were imported with `-Plant`. The current contact doses are 0.5 need units. The creek pool begins at 1,000,000 units and each imported shrub starts with 2 units; both pools are finite and never go below zero.

Each E hold opens one bounded contact session. A water contact and each shrub contact can each transfer at most once during that session. Releasing and pressing again starts a new session. The normal interaction path does not move the hero; `-DiagnosticNeeds` is a bounded test route that steers to one imported shrub and then the creek, simulates one press at each eligible contact, and exercises the same contact checks.

## Saves and remaining work

Current hero needs are restored from STL4 needs snapshots and written with the existing save. Older save formats without a needs snapshot keep the existing zero defaults. A selected successor starts with default needs instead of inheriting the dead hero's values. Resource pools and the contact ledger remain run-local and are not persisted.

Browsing and drinking currently change logical needs only. Dedicated head and neck animations are missing. Host walking still uses the existing prescribed speed and does not extract root motion from the imported clip, so this interaction work does not establish foot locking or gait fidelity. Full-draft work remains for integrated gameplay, broader creature and flora behavior, ecology, relationships, parenting, reproduction, and player-facing feedback.

Diagnostic receipts report contact eligibility, applied contact counts, before/after needs, finite resource balances, save/load state, and sequence stage. These are feasibility observations; they do not establish accepted visuals, full draft completion, or target frame rate.
