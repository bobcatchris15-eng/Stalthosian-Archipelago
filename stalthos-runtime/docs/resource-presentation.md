# Resource Presentation

The `stalthos::ShrubPresentation` acts as a pure bounded visual-depletion presentation contract for shrub resources, translating internal biological resource quantities (from `ResourceLedger`) into explicit rendering parameters, ready for later Wicked integration.

## Integration Points

The integration operates downstream from the existing `ResourceLedger`.

1. **Host query:** The game host reads the current `ResourceDto` from `ResourceLedger`.
2. **Visual evaluation:** The host uses `ShrubPresentation::evaluate(ResourceDto, optional_previous_state)` to calculate the `ShrubVisualState`.
3. **Application:** The host applies this discrete visual state (like foliage visibility/scale, and leaf cluster counts) to existing imported plant objects.

## Visual Representation Contract

The contract derives foliage visibility/scale or density state purely from the authoritative initial and consumed quantities.

- **Independent of Budget:** The contract operates independently of the global resource budget, relying completely on the provided resource snapshot to avoid synchronization skew.
- **Deterministic & Frame-Independent:** Computations do not depend on framerate, randomization, or independent time states, remaining completely host-agnostic and deterministic.
- **Finite Validation:** Inputs are rigorously validated for non-finite values (NaN/Infinity), negative quantities, or over-consumption prior to calculations. Invalid queries will safely return a structural null response (i.e. `valid = false`).
- **Woody Stem Preservation:** The primary woody stem remains visible even at complete exhaustion, providing continuous structural context on the landscape.
- **Foliage Hysteresis:** If caller-state is provided, the calculation uses hysteresis (a 0.1-cluster margin beyond the rounding boundary) when quantizing leaf clusters (e.g. 0 to 4 quarters). This mitigates visual flickering for resources hovering strictly around fractional capacity boundaries due to gradual or micro-consumption.
- **Creek Exclusion:** Presentation logic specifically excludes logic for `ResourceKind::Creek` or unhandled resource kinds, enforcing separation between plant models and ground-fluid presentation.

## Persistence

Do not implement a new persistence schema for this visual presentation. Persistence is exclusively owned and governed by `ResourceLedgerDto` and `ResourceSessionDto`. `ShrubVisualState` acts purely as transient diagnostic presentation state. The foliage fraction reconstructs exactly from the resource snapshot. Optional transient hysteresis may produce a different cluster count at a rounding boundary after load; omit previous state for exact count reconstruction.

Parent review: tests use explicit runtime checks active in Release; malformed previous hysteresis state is ignored. This helper is not yet wired into imported foliage and does not establish visible depletion.
