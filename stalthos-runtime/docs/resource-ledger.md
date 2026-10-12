# Resource ledger

`domain/resource_ledger.hpp` provides a bounded, engine-independent C++17 pool
for finite resource quantities. Resource IDs and interaction session IDs are
stable `uint64_t` values supplied by the caller. Shrub and Creek are resource
kinds, not claims about simulation fidelity.

The default limits are 4096 resources and 256 successful interaction keys.
History never evicts old keys: a new successful interaction throws
`std::runtime_error` when the history cap is full. Replays with the same
`(sessionId, resourceId, amount)` succeed without another deduction; reusing a
key with a different amount returns false. Invalid amounts throw
`std::invalid_argument`. Insufficient or unknown resources return false and do
not alter quantities or history.

`load(vector<ResourceDto>)` is a transactional quantity-only load that starts a
fresh interaction-history epoch. It is appropriate for initial world state,
but does not preserve replay protection from a prior ledger instance. For
save/restore continuation, serialize `continuationSnapshot()` and call
`restore(ResourceLedgerDto)`. That DTO includes sorted resource quantities and
the bounded session history; restore validates the complete DTO before changing
the receiver. Plain `snapshot()` contains only resource quantities and must not
be described as a continuation snapshot. Neither DTO is wired into runtime
persistence yet.

Resources are finite and do not regenerate. `reset()` explicitly clears both
resources and replay history; no regeneration or new-world generation is
implied. Snapshots sort resources by ID and sessions by `(sessionId, resourceId)`
for deterministic output.

Standalone offline check (from this directory's parent):

```powershell
g++ -std=c++17 -DNDEBUG -Wall -Wextra -Werror -pedantic tests/resource_ledger.cpp -o resource_ledger_test
./resource_ledger_test
```

The tests use active checks independent of `assert`/`NDEBUG`. Root integration
may wire this test target into CMake later; this standalone addition does not
modify build configuration or claim integrated save support.

Root integration adds domain-resource_ledger to CMake/CTest. The root Release suite passes 12/12; actual host save/restore wiring remains pending.
