# Fauna population implementation receipt

- Branch: `codex/fauna-population-20261017`
- Starting commit: `49ad23a` (`HEAD` when isolated worktree was created)
- Scope: only `stalthos-runtime/domain/fauna_population.hpp`, `stalthos-runtime/tests/fauna_population.cpp`, this task receipt, and `stalthos-runtime/docs/fauna-population.md`.
- API: requested `Activity`, `HabitatSite`, `ActorView`, and `Population` constructor / `advance` / `actors()` contract.
- Behavior: deterministic IDs with reserved/excluded checks; active-region cap 16, habitat cap 5000; connected caller-supplied route graph; actual site starts; seeded browse/drink/rest/travel; shared Lifecycle and NeedsCore; physical-time route movement and fatigue; calendar-time age and hunger/thirst; staged update before commit; no persistence.
- Verification command: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build var\fauna-tests\build --config Release` followed by `var\fauna-tests\build\Release\FaunaStandalone.exe`.
- Result: Release build succeeded with `/W4 /WX /permissive-`; executable printed `fauna population checks passed: 6 actors, deterministic motion, bounded routes, core time separation` and exited 0. The test does not rely on `assert`, so checks remain active under `NDEBUG`.
- Checks cover deterministic repeats, mixed visible initial activities, valid unique/excluded identities, supplied-site spawn positions, dt=0, negative/nonfinite atomic rejection, bounded route movement and ledger, small-step site arrival, physical/calendar separation, activity at resource-eligible sites, empty/count cap and invalid-neighbor handling.
- Root CMake, runtime main, persistence, renderer, source assets and planning inputs were not edited.

SHA-256:

| File | SHA-256 |
|---|---|
| `stalthos-runtime/domain/fauna_population.hpp` | `1906919437B6DD597C05006A2D69CAA88A9352D89AEA3D56031217B711D73EA4` |
| `stalthos-runtime/tests/fauna_population.cpp` | `3DD6451A158DEB42473E47EFD97D326960F7D4A6FD42AD1900F004F07AF6838B` |
| `stalthos-runtime/docs/fauna-population.md` | `0E1005A78351A159E2C29E5411FF6E61F432AF4135829872CC2878D23BBCDF83` |
