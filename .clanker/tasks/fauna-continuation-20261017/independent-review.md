# Population continuation independent review — 2026-10-18

Read-only reviewer reproduced a generated midroute-death snapshot rejection and identified an unversioned STL7 mapping extension. Root fixed both. Reviewer independently recompiled fauna and persistence tests with MSVC in temporary storage: fauna checks passed, persistence 20/20 passed. No remaining blocking findings in the bounded rereview.

Dead actor positions are now checked against their saved route segment; off-route corpses still fail. Writer emits STL8; original STL7 layout is decoded without mapping bytes and migrates to STL8. Explicit prior-layout fixture and generated death regression pass.

Resource bridge restores actor/session mapping and cumulative ledger amounts before new contacts; next session ID accounts for persisted mappings. Native replay is root-owned evidence, not reviewer visual or performance acceptance.
