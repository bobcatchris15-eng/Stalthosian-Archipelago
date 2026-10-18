# Coherent native regional save integration

Status: **Proposed design; not implemented.** The original watershed ownership
adapter can continue capturing its one retained population, complete shared
ledger and explicit actor/session mappings in STL8. That does not persist an
expanded regional registry. Do not enable additional retained regions in the
host until a complete save transaction exists.

## One transaction, one file

Use a versioned host container holding both the legacy host state and the full
regional continuation. Prefer a new STL9 container over an independently
replaced STL8/STLR pair. The container includes an exact STL8 payload and an
STLR payload, bounded lengths, container version, world seed, immutable world
configuration revision and a generation identifier. A checksum covers the
header and both payloads to detect accidental corruption; it is not an
authentication mechanism. Reject unknown versions and trailing bytes.

Keep STL1-STL8 import behavior and an explicit one-region STL8 export path.
Existing readers cannot read STL9; do not describe the new container as backward
readable. Never silently downgrade a multiregion save to STL8 or discard other
regions. A plain STL8 import migrates only its actual saved watershed animals;
coarse island capacities must not become invented retained individuals.

## Capture and validation

Capture the host and registry at one simulation boundary, with no advance or
consumption between their snapshots. The registry is the sole authority for
population and resource state. Derive the legacy payload's population, ledger
and session mappings from that same registry snapshot. Bind the host hero region,
seed and simulation clocks to the container's world identity and generation.

Before writing, require exact agreement between the legacy region in STLR and
the STL8 population, complete ledger and explicit session mapping. Validate the
full container against the admitted immutable world configuration. Limits apply
before allocations: the existing 64 MiB legacy bound, 512 KiB regional bound,
eight regions, 64 individuals, 4096 resource reservations and 256 history keys,
plus a small fixed-size header. A checksum never replaces semantic validation.

## Replacement and recovery

Encode and validate the entire candidate before touching the current save.
Write a uniquely named temporary sibling on the same volume, flush and close,
then read it back through the normal container validator. Publish with a tested
Windows replacement primitive for an existing destination and an atomic
same-volume move for a new destination. Check every failure and retain the prior
save on failure. Do not remove the destination as a replacement fallback.

The existing STL8 writer uses filesystem rename and may fail when a Windows
destination already exists. It does not establish an overwrite contract for the
future container. Likewise, stream flush and atomic namespace replacement do
not establish power-loss durability. Any stronger durability claim requires
explicit OS/filesystem testing. Ignore orphan temporary candidates on load;
never load an arbitrary newer temporary generation automatically.

## Restore without a half-applied world

Recognize the outer format first. For STL9, decode both bounded payloads into
detached objects, check their binding and shared legacy state, and construct an
equivalent candidate world using the external immutable configuration. Resource
IDs, initial quantities, habitat topology/signatures, species, anchors and
reservations must match that configuration. Validate saved actor/session
provenance explicitly; history alone cannot identify an actor. Restore the
registry continuation and host state only on detached candidates, then swap the
complete host-world owner at one boundary. Any error leaves the running world
unchanged and reports an actionable load failure.

No fallback to a mismatched companion, no regenerated retained population and
no partial region activation are allowed. Hero and NPC consumption continue to
use the same registry authority after restoration.

## Required evidence before adoption

- Import an existing STL8 save and prove exact original-eight snapshots, session
  ownership and complete depletion/history, including hero bookkeeping keys.
- Save, exit and reload a native world after departure, elapsed dormancy and
  return across at least two real rendered habitats; demonstrate continued
  individual identities and observable resource depletion.
- Reject mismatched host/registry payloads, worlds, configurations, generations,
  checksums, truncations and oversized counts without changing live state.
- Inject write, close, readback and replacement failures; prove an existing save
  remains readable and unchanged. Test a preexisting destination on Windows.
- Retain legacy format fixtures and explicit multiregion export refusal.

These are acceptance conditions for future implementation. This document does
not claim disk integration, crash recovery, native multiregion gameplay, visual
acceptance or measured frame rate.
