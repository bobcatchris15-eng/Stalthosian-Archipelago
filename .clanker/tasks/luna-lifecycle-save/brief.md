# Bounded implementation brief

task_id: luna-lifecycle-save
objective: Add a validated optional current-hero lifecycle snapshot to native save DTOs and versioned persistence, while preserving STL1/STL2 read compatibility and existing captureSave/restoreSave signatures. Provide explicit helpers for host lifecycle capture/restore and document migration behavior and unfinished scope.
acceptance:
- Save DTO can carry hero identity, species, calendar age, and stage; invalid IDs, profile-stage-age contradictions, and dead stage for a live hero are rejected before previous-save modification.
- New writes use STL3 with exact bounded size accounting; reads accept valid STL1/STL2/STL3 and reject malformed, truncated, unsupported, or trailing data.
- Legacy lifecycle absence remains explicit; helper restores age 0 only using caller-supplied species and does not infer legacy age.
- Tests cover lifecycle roundtrip, contradiction/malformed/truncation/version migration, old formats, and prior-save safety.
- MSVC Release domain build/tests run successfully and output is preserved.
- lifecycle-persistence.md documents wire fields, validation, migration policy, dead-hero policy, and unfinished needs/bonds/reproduction.
artifacts: stalthos-runtime/domain/persistence.hpp; stalthos-runtime/tests/persistence.cpp; stalthos-runtime/docs/lifecycle-persistence.md; .clanker/tasks/luna-lifecycle-save/
constraints: Keep captureSave/restoreSave APIs compatible. Do not edit engine/main.cpp, Run.ps1, toolkit/core, planning clone, or unrelated files. Do not commit, push, or spawn workers.
authority: This worktree and task state only; owned implementation files are the three listed source/doc paths.
returns: C:/Users/Chris/.codex/skills/clanker-mode/contracts/clinker-receipt.schema.json
budget: bounded implementation; finish before dispatcher deadline 2026-10-11 17:32 America/Chicago.