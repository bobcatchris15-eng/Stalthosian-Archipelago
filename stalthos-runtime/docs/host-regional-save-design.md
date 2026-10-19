# Coherent native regional save integration

Status: **Native STL9 adoption implemented for one original watershed; validation receipts establish the tested scope.**
The populated native host writes one STL9 file containing host and full regional
continuation. Legacy STL1-STL8 imports remain supported. Additional retained
regions remain disabled pending exterior construction and admission migration.

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


## Historical foundation checkpoint - 2026-10-18

The following records the library-only stage before native adoption below.

`domain/host_save.hpp` implements the bounded STL9 container and single-file
transaction as a library. The native executable still reads/writes STL1-STL8
through its established path. No native multi-region adoption or new gameplay
acceptance is claimed. Existing legal legacy imports remain supported, including
empty populations and legal saved actor species/identities.

The container has a 72-byte little-endian header (magic/version, external
configuration identity, nonzero generation, seed, physical/calendar clocks,
hero region, payload lengths), exact STL8 and STLR payloads, and an eight-byte
FNV-1a checksum covering header and payloads. This checksum detects accidental
corruption; it provides no authentication or anti-rollback guarantee. Generation
is transaction metadata, not a separately persisted payload generation. The
caller owns configuration versioning and captures both snapshots at one boundary.
Unknown versions, non-STL8 embedded hosts, bounds, trailing bytes, header/host
identity/clock disagreement and full legacy-region state disagreement reject.

API distinction is deliberate:

- `decode`/`read` and `encode` validate syntax, checksum and mutual payload binding.
  They return detached data; they do not admit immutable world configuration.
- `restore(candidate, admittedRegistry, expectedConfiguration)` copies an externally
  admitted registry, validates the full continuation on that copy, reconstructs
  host state, and returns a detached `RestoredWorld` containing clock, host and
  registry. The caller replaces its complete live owner only after this succeeds.
- `write(path, candidate, admittedRegistry, expectedConfiguration, failure)` requires
  that same external semantic admission before creating a temporary sibling and
  after normal readback. It uses Windows `ReplaceFileW` for an existing destination
  and `MoveFileExW` without replacement for a new destination. It never deletes
  the destination as fallback. Stream flush/close plus namespace replacement does
  not establish power-loss durability.
- `exportLegacy` is explicit and refuses a multi-region DTO before touching disk.
  Its existing STL8 writer retains its prior Windows overwrite limitations.

The foundation review found that constructing an admission registry from the
candidate's population does not independently prove historical species/identity:
a coherent change in both payloads becomes its own reference. Initial wiring
that claimed such independent proof was removed. The native adoption policy
below resolves compatibility explicitly: world definitions remain independent;
individuals and imported resource quantities are validated save declarations,
with no claim to historical authentication. Legal legacy admissions are not
restricted to the generated eight.

Windows domain tests cover exact container roundtrip, existing destination
replacement, real sharing-lock replacement failure, injected write/close/readback/
replace failures retaining prior bytes and readability, semantic species/resource/
anchor rejection against external admission, legacy compatibility, and explicit
multi-region export refusal. The four failure hooks test transaction stage aborts;
they do not emulate every device/OS failure mode. No native restart, rendered
habitat travel, power-loss recovery, visual quality or frame-rate proof follows
from these library tests.


## Native adoption policy - 2026-10-18

The native populated host now captures STL9 in one boundary after simulation stops:
legacy host state, complete population/ledger/session mappings and registry
continuation derive from the same authority without an intervening advance or
contact. Non-populated diagnostics retain their prior STL8 writer. Native STL9
restoration supports exactly watershed1001 and configuration revision2, including
the populated-mode identity. Additional retained regions remain disabled.

Admission distinguishes independent world configuration from save declarations:

- Runtime construction supplies habitat topology/signature, anchored resource
  IDs/kinds and anchor positions, supported species catalog, reservations and caps.
- Legal saved individual IDs/species, actor state, session ownership, imported
  resource initial quantities, extra legal resources and complete depletion/history
  are **validated save-declared continuation**, not authenticated historical truth.
  This preserves the prior native legacy import behavior, including empty animals,
  non-generated legal individual admissions and noncanonical legal quantities.
- Exact payload binding, supported enums, uniqueness/exclusions, finite bounded
  state, ledger invariants and anchored-resource compatibility establish internal
  consistency. A coherently edited legal individual/resource declaration remains
  acceptable by this policy. Neither checksum nor re-encoding proves authenticity.

This explicitly supersedes the earlier unresolved requirement for an independently
reconstructed historical individual/species baseline. Reconstructing from admitted
save declarations is now a documented compatibility policy, not independent proof.
No trust store, cryptographic signature or anti-rollback claim is introduced.
Telemetry reports validated_save_declared versus generated/native_defaults,
historical_identity_authenticated=false, and separate loaded/saved generations.

Loading is startup-only. Host fields are decoded/validated to initialize rendered
world construction; after external habitat/anchors exist, complete detached
clock/world/registry ownership is reconstructed and assigned as one owner before
any simulation/contact tick. Rejection exits startup; no live in-session reload
API is added. Explicit successor selection is applied after that restoration so
it cannot be overwritten by the saved terminal state. Lifecycle/needs/controller
initialization uses the already validated host snapshot under the existing startup
path. The sole ledger owner remains the registry after bootstrap release.

The library's admission parameter still enforces whatever immutable individual
provenance a caller supplies. The native caller deliberately supplies declared
individual provenance plus independently constructed world configuration. Generic
library tests demonstrating rejection against a separately known species baseline
remain valid; they do not imply native historical species authentication.
