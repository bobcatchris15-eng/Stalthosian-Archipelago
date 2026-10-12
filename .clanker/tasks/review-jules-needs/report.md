# Jules needs review

Outcome: done. Jules session 4917980815644905444 was retrieved after checking CLI help and applied in isolated worktree `var/worktrees/review-jules-needs` on `codex/review-jules-needs`, based on `main` at `aa3f3c8`. The pull contained exactly three paths: `stalthos-runtime/domain/needs.hpp`, `stalthos-runtime/tests/needs.cpp`, and `stalthos-runtime/docs/needs-draft.md`.

Review correction: the header rejected any timestep larger than `double::max()/10`, even when all computed need values remained finite. Removed that arbitrary cutoff. The test now exercises a `double::max()` finite physical and calendar delta and confirms finite needs saturate at 1.0. Documentation describes this behavior. Trailing whitespace was cleaned.

Validation ran offline with VS 18 MSVC from the worktree runtime directory:

`call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cl /nologo /std:c++17 /W4 /WX /EHsc /permissive- /Fo:"E:\Stalthosian Archipelago\var\reports\review-jules-needs\needs.obj" /Fe:"E:\Stalthosian Archipelago\var\reports\review-jules-needs\needs.exe" tests\needs.cpp && "E:\Stalthosian Archipelago\var\reports\review-jules-needs\needs.exe"`

Output: `41 tests passed. 0 tests failed.` Log: `var/reports/review-jules-needs/test-run.log`.

Invalid-state checks: constructor rejects unknown species; `advance` rejects negative and non-finite time before calculation; computed values are checked for finiteness before assigning state; interventions reject negative/non-finite amounts; `restoreState` validates all four normalized values before writing any field. This preserves state for rejected input. Huge finite values are accepted if their scaled calculations remain finite and clamp to normalized bounds.

SHA-256 of tested sources:

- `domain/needs.hpp`: `0277247930AF66D8132C30770423CD1B20F3B78949A0F7865D0087F6F42D7229`
- `tests/needs.cpp`: `D879433E520F58186A6AD1114E289D4009DCFA0D4505B40E0491B482CBF6BEEB`
- `docs/needs-draft.md`: `54E855BBE780BFA2E4506C5C8011BC705081EA0093B1574C77B98A3BF84D422C`

Integration note: CMake does not currently register `tests/needs.cpp` in CTest. `CMakeLists.txt` was outside this task’s ownership and was left unchanged. No primary-checkout source files were changed by this task.