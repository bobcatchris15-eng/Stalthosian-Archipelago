# Jules relationship foundation review

Jules session `7316297015894091861` was pulled after checking `jules remote pull --help`. The raw returned diff is preserved at `var/jules-patches/review-jules-relationships.patch` (SHA-256 `80BEBA68B0D7108F3DDAFB2BC0ADD2F5FE886EB7297C66E8B129BC372730DB56`). The patch was applied only in `var/worktrees/review-jules-relationships`, branch `codex/review-jules-relationships`, based on `main` at `c93b264e5d2571b844ac8b0a0c6282c3c52ac2ab`.

The review found that confirmed death was represented only by one observer's link, allowing a new relationship to treat the same identity as present. It also found that a zero `maxLinksPerNode` limit was bypassed when both relationship endpoints were new. The candidate now keeps confirmed death as identity truth in a ledger bounded by the existing graph node limit, exposes `isConfirmedDead`, rejects future present/separated observations and new links involving a dead identity, and checks the zero-link capacity. Relationship formation stages both sides in a bounded graph copy and swaps them in together. Directional separation remains on the observer's edge. `Identity` is `uint64_t` with no reserved zero value in `foundation.hpp`; a regression test records that ID zero is accepted.

Regression coverage also checks absent/lost-contact observations do not create identities, backwards and non-finite observations preserve prior state, post-death operations preserve the graph, and reciprocal/caregiver roles and configured limits behave as intended. No removals/restoration API exists; confirmed death has no clearing operation.

Validation passed offline using Visual Studio Build Tools 18:

```powershell
cmd.exe /d /s /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cl.exe /nologo /std:c++17 /O2 /DNDEBUG /EHsc /W4 /permissive- /D_HAS_EXCEPTIONS=1 stalthos-runtime\tests\relationships.cpp /Fe:var\relationships-review-release.exe /Fo:var\relationships-review-release.obj /Fd:var\relationships-review-release.pdb && var\relationships-review-release.exe'
```

Output: `relationships.cpp` then `All tests passed.` `git diff --check` returned no diagnostics; final trailing-whitespace scan of all three candidate files returned no matches. CMake integration and merged-tree validation remain with root.

Integration paths (copy from the reviewed worktree):

- `stalthos-runtime/domain/relationships.hpp` — SHA-256 `A7C8FD80D839197B64A6DA6EB615524F5842A033878785D59339FDA21B86C1ED`
- `stalthos-runtime/tests/relationships.cpp` — SHA-256 `20BFBC4FAE7C0A54D8894DA9E1A48459BD3FD315B2B0791F268410BF2497C71A`
- `stalthos-runtime/docs/relationships-draft.md` — SHA-256 `79FFB18FE2599D41825F3B43BDA9A6F0BE8309038F56D279D5AFB2C602695B01`

The required `.clanker/mode/contracts/prelude.md` and `.clanker/mode/roles/clinker.md` paths were absent; the paths under `C:\Users\Chris\.codex\skills\clanker-mode\` were present and read. The authoritative ownership and acceptance brief was `.clanker/tasks/jules-relationships/brief.md`. No primary runtime source or CMake files were edited.

