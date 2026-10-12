# Jules activation planner review

## Result

Reviewed Jules session `16396772999683175155` in isolated worktree `var/worktrees/review-jules-activation`, based on `main` at `c93b264e5d2571b844ac8b0a0c6282c3c52ac2ab`. The source scope is exactly the three assigned files. Review changes are committed on branch `codex/review-jules-activation` as `4b58ea58609819607cef730fa9aebbd6c4995381`; no source changes were made in the primary checkout.

## Review changes

- Replaced squared-distance comparisons with finite-checked 3D `std::hypot`, and reject derived distance or radius overflow before a plan can be returned.
- Bound the planner to 100,000 regions and each budget to the same ceiling.
- Documented sparse output: omitted IDs are assigned `Distant`, so consumers must downgrade old physical or presentation staging. Added a regression test for a prior physical region outside both radii.
- Added tests for overflow, budget bounds, input immutability, and the sparse downgrade behavior. Existing tests cover body/camera separation, deterministic distance/ID ties and input ordering, hysteresis, budget saturation, invalid inputs, duplicate IDs, and invalid tiers.
- The planner API only accepts positions/configuration/regions and returns tier plans; it has no perception or stimulus mutation path.

## Validation

Command, run from the review worktree:

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cl /nologo /std:c++17 /EHsc /W4 /WX /permissive- /O2 /DNDEBUG stalthos-runtime\tests\activation_plan.cpp /Fe:activation-plan-final.exe && activation-plan-final.exe'
```

Result: Visual Studio 2026 x64 environment initialized; compile completed with `/W4 /WX`, optimized Release flags `/O2 /DNDEBUG`, and the test executable printed `All tests passed.` Offline, standalone C++17 test only; no CMake/runtime host integration or engine/performance acceptance was attempted. Executable/object evidence is retained under ignored `var/reports/activation-plan-tests-review-final.exe` and `var/reports/activation_plan-review-final.obj`.

Whitespace validation: `git -c core.whitespace=cr-at-eol show --check --oneline --stat HEAD` completed without reported whitespace errors. The `cr-at-eol` setting accommodates the repository's Windows checkout line endings.

## Hashes

- `stalthos-runtime/domain/activation_plan.hpp` — SHA-256 `04FA14DED0D57A4B999ABF2088C6131A7E4CD1CD7B25B299E2029A00140A73CA`
- `stalthos-runtime/tests/activation_plan.cpp` — SHA-256 `7F203BE0D60747031C2E98EB075643F8BD231AB528352C131E16034BF4BD836C`
- `stalthos-runtime/docs/activation-plan-draft.md` — SHA-256 `9DA51B024668385A5E16DFA8D86451C5571380E5A04DA2A78656AB9AF240828B`
- Retrieved Jules patch at `var/reports/jules-activation-final.patch` — SHA-256 `30554087091788B1B29FA75C5A44932A96FC044005D8A74B78BDE40809FF04BC`

## Integration

Cherry-pick `4b58ea58609819607cef730fa9aebbd6c4995381` into `main`, or take only the three committed paths. Root must wire the test target into CMake. Jules did not modify CMake, host, persistence, toolkit, or lifecycle files. The draft still lacks source population reconstruction/catalog, actual Wicked streaming, persistence/host integration, final tuning, measured frame-time acceptance, and the rest of the full-draft gameplay scope.
