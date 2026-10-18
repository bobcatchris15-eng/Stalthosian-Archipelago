# Clinker receipt: fauna-continuation-20261017

Outcome: **done** within assigned scope. Changes are in the fauna population, persistence, tests, and documentation paths listed below. The bounded task is complete; root owns host wiring and broader world replay validation.

## Acceptance

- Atomic fauna snapshot/restore with seed, region, ordered habitat position/topology signature, IDs/species, exact position/heading/distance/site/target, age/stage, needs/injury/resting, and activity: **met**. `restore(snapshot, excludedIds)` validates and builds replacement state before swapping. Seed/region/signature mismatch, invalid enums and profile/state combinations, non-finite values, invalid indices, wrong stationary positions, off-route travel positions, and identity conflicts fail closed.
- Continuous versus save/restore deterministic continuation: **met** by the fauna test comparing actor routes, activity, motion, age, needs, and resting state exactly over continued timesteps.
- Optional `SaveData.faunaPopulation`, STL7 writer, and readers for STL1 through STL6: **met** by STL7 roundtrip and explicit STL6 fixture compatibility tests, plus the existing STL1–5 fixture tests.
- Limits and strict actor identity checks: **met** for 16 actors, site indices under the 5000-site cap, duplicate IDs, global hero collision, retained/dead identity collision through save validation, invalid species/stage/activity, invalid needs/lifecycle, invalid payload presence/count/species, and truncation.
- Root’s additional FaunaResources session-history persistence: **not part of this commit**. Resource ledger/session reuse remains owned by root’s integration work; the population payload does not itself serialize historical resource consumption sessions.

## Evidence

Configured the domain-only Release build with Visual Studio 18 2026 and `-DSTALTHOS_BUILD_WICKED_HOST=OFF`.

Command:

```powershell
& 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' -S stalthos-runtime -B var/build/fauna-continuation -G 'Visual Studio 18 2026' -DSTALTHOS_BUILD_WICKED_HOST=OFF
& 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' --build var/build/fauna-continuation --config Release --target Stalthos_fauna_population StalthosPersistence
& 'var/build/fauna-continuation/Release/Stalthos_fauna_population.exe'
& 'var/build/fauna-continuation/Release/StalthosPersistence.exe'
```

Observed output: fauna population checks passed; persistence reported **19/19 tests passed**. `git diff --check` passed.

## Artifacts changed

- `stalthos-runtime/domain/fauna_population.hpp`
- `stalthos-runtime/domain/persistence.hpp`
- `stalthos-runtime/tests/fauna_population.cpp`
- `stalthos-runtime/tests/persistence.cpp`
- `stalthos-runtime/docs/fauna-population.md`

The task checkpoint is `task-state.json`. No planning inputs, CMake files, host files, or unrelated work were changed.
