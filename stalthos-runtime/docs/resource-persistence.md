# Resource Persistence

This document specifies the persistence contract, binary format, and runtime integration for ecological resources in the Stalthosian Archipelago (STL5 format).

## Format Specification (STL5)

The runtime records ecological resource pools and interaction continuation history in `SaveData` via `std::optional<ResourceLedgerDto>`. In save version 5 (magic `STL5`, version `5`), the resource ledger is serialized immediately following the `heroNeeds` optional block and before `activeTransients`:

- **1 byte**: presence flag (`0` = omitted / legacy default pools, `1` = present). Value `> 1` is rejected as corrupt.
- When present:
  - **4 bytes**: resource count $N$ (`uint32_t`, bounded to $\le 4096$ and $N \times 25 \le \text{fileSize}$).
  - **$N \times 25$ bytes**: resource records sorted by `id`:
    - 8 bytes: resource `id` (`uint64_t`)
    - 1 byte: `kind` (`uint8_t`, `0` = Shrub, `1` = Creek; `> 1` rejected)
    - 8 bytes: `initialQuantity` (`double`, finite, $\ge 0$)
    - 8 bytes: `consumedQuantity` (`double`, finite, $0 \le \text{consumedQuantity} \le \text{initialQuantity}$)
  - **4 bytes**: session history count $M$ (`uint32_t`, bounded to $\le 256$ and $M \times 24 \le \text{fileSize}$).
  - **$M \times 24$ bytes**: session interaction records sorted by `(sessionId, resourceId)`:
    - 8 bytes: `sessionId` (`uint64_t`, $> 0$)
    - 8 bytes: `resourceId` (`uint64_t`, references known resource ID)
    - 8 bytes: `amount` (`double`, finite, $> 0$)

## Validation and Transactional Guarantees

1. **No Divergent Validator**: Save validation in `persistence::detail::validate` delegates resource ledger invariant checking directly to `ResourceLedger::restore(dto)`.
2. **Transactional Write**: `writeSave` executes full validation before touching destination or temporary files. Any malformed input (corrupt kind, negative/non-finite quantities, unknown resource references, duplicate IDs, session ID 0, over-consumed quantities, or cap violations) causes an immediate exception and guarantees the pre-existing file on disk remains unmodified.
3. **Transactional Restore**: `restoreResourceLedger` checks all invariants into isolated staging containers before mutating live state.

## Session IDs, Idempotence, and Replay

- **Session Monotonicity**: Persisted session IDs are retained in continuation history. On host startup or load, the engine inspects the persisted session list and sets the next interaction session ID strictly greater than `max(persistedSessions)`. If the maximum persisted session ID reaches `UINT64_MAX`, the host triggers an overflow failclean error rather than wrapping.
- **Resumed Process vs. Live Hold**:
  - Resuming a new process with an actively held interaction key initiates a new session with an ID strictly greater than the saved history.
  - Repeated frames during the same continuous live hold share the identical `physicalInteractionSession`. Idempotent replay in `ResourceLedger::consume` returns `true` for exact replays without deducting additional quantities, and `InteractionsCore::applyContact` suppresses duplicate transfers within the same session.
- **Authoritative Consumption**: Need reduction (`drink()` / `feed()`) only occurs upon successful ledger deduction (`actualAmount > 0 && ledger.consume(...)`). Partial available amounts are supported via `min(request, available)`.

## Successor and Cross-Life Depletion

Resource pools belong to the landscape, not the ephemeral hero. When a hero dies and a successor is selected (`--successor-id`):
- Depleted resource quantities (e.g. consumed shrub browse or water intake) are restored intact from the world save.
- Successor interactions begin from a fresh session epoch strictly greater than the predecessor's recorded sessions.

## History Cap (256 Sessions) and Graceful Exhaustion

`ResourceLedger` enforces a hard ceiling of 256 history entries (`maxHistory = 256`) to ensure deterministic bounding.
- When capacity is reached, `consume()` throws `std::runtime_error("ledger history capacity exceeded")`.
- The engine catches this exception gracefully: **no process crash and no silent eviction of history**.
- Further consumption attempts fail closed without reducing needs.
- The event is recorded in the diagnostic receipt counter `resource_ledger_cap_exhaustions` and documented in `limits`. This cap is a bounded testing constraint, not a completed production game feature.

## Backwards Compatibility

When reading STL1, STL2, STL3, or STL4 saves:
- The parser does not read a resource ledger block; `data.resourceLedger` remains `std::nullopt`.
- `restoreResourceLedger` resets the ledger to empty (`ledger.reset()`).
- The engine identifies that the loaded save lacks a ledger and initializes standard default pools (`creek 100 = 1,000,000.0`, `shrubs 200..207 = 2.0`).

## Verification Receipt

Native MSVC cl.exe (Visual Studio 2026 BuildTools v18.6) test run:
- **Command**: `cl /std:c++17 /EHsc /O2 /W4 /WX /permissive- /DNDEBUG stalthos-runtime\tests\persistence.cpp /Fe:test_persistence.exe && test_persistence.exe`
- **Results**: 15/15 tests passed:
  1. `PASS roundtrip and deterministic ordering`
  2. `PASS write and restore validation`
  3. `PASS malformed files and aggregate bounds`
  4. `PASS write failure preserves prior save`
  5. `PASS restore checks mutation statuses`
  6. `PASS authoritative capture through file roundtrip`
  7. `PASS STL1 fixture compatibility`
  8. `PASS STL3 lifecycle roundtrip and validation`
  9. `PASS STL2 migration and STL3 corruption`
  10. `PASS physical time and pose validation`
  11. `PASS STL4 needs roundtrip and validation`
  12. `PASS STL4 legacy compatibility`
  13. `PASS STL5 resource ledger roundtrip`
  14. `PASS corrupt resource ledger rollback and size caps`
  15. `PASS saved depleted quantities restore across lives`
Root combined Release build and CTest14/14 passed. Actual Wicked runs 20261012-105354-7f3bc9f0 and 20261012-105605-b78294c2 consumed0.5 per resource on fresh sessions across restart: food2->1.5->1, creek1000000->999999.5->999999. A third idle reload20261012-105850-e0f08db3 retained those final quantities without refill. Plant fixture is generated basketfern, diagnostic imported-plant coverage only; visible depletion remains unimplemented.
