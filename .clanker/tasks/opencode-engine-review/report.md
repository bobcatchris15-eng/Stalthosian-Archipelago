# Fresh-engine build entry point — proposal

## Current state (verified from files)

- `stalthos-runtime/CMakeLists.txt:19-24` globs `${WICKED_RELEASE_CACHE}/objects/*.obj` and hard-fails unless exactly 80; those `.obj` are marked `EXTERNAL_OBJECT GENERATED` and linked directly into `Stalthos` (`:28`). Nothing in the repo ever compiles Wicked Engine sources.
- `stalthos-runtime/CMakeLists.txt:25-27` hand-rewrites `Editor/ModelImporter_GLTF.cpp` at configure time (replacing `#include "stdafx.h"` with `WickedEngine.h`) — a host-side shim that a real engine build would supersede.
- `Prepare-Wicked.ps1:25-61` only *copies* prebuilt inputs. The receipt at `:59` literally says `note='...not a fresh engine rebuild'`. Its gate at `:28` enforces the exact cache contract: `WICKED_EMBED_SHADERS=ON`, `WICKED_DYNAMIC_LIBRARY=OFF`, `USE_AVX=ON`, `USE_AVX2=OFF`, `CMAKE_GENERATOR=Visual Studio 18 2026`, and `WickedEngine_common.dir/Release/*.obj` count == 80.
- `Build.ps1:19-29` re-verifies the 85-artifact manifest (80 obj + 4 libs + `dxcompiler.dll`) by SHA-256 on *every* build; `:24-26` require an exact, unique set match. `CMakeLists.txt:40` copies `bin/dxcompiler.dll` beside the exe.
- `config/wicked.lock.json` pins revision `df44c3db4c4927492bc9c791eac715d98d7ed091`, tag `v0.72.120`, `avx2_required: false`, DX12.
- `README.md:26` states plainly: "A self-contained fresh-engine compilation entry point remains a construction task."

## Smallest concrete proposal

**One new script: `stalthos-runtime/scripts/Build-Wicked.ps1`.** No change to `Build.ps1`, no change to `CMakeLists.txt` default path. The fresh build *produces the same cache layout*, so every existing verification path keeps working unchanged.

### 1. Configure (mirror the cache contract exactly)

```
cmake -S var/vendor/WickedEngine -B var/build/wicked-engine ^
      -G "Visual Studio 18 2026" -A x64 ^
      -DWICKED_EMBED_SHADERS=ON -DWICKED_DYNAMIC_LIBRARY=OFF ^
      -DUSE_AVX=ON -DUSE_AVX2=OFF
cmake --build var/build/wicked-engine --config Release --target WickedEngine
```

Rationale for each flag: they are copied verbatim from the `Prepare-Wicked.ps1:28` gate, so the output is *provably* the same configuration the 85-input path was verified against. Do not add `WICKED_CMAKE_BUILD` etc. here — those are host-target compile definitions (`CMakeLists.txt:33`), not engine options.

**Unverified / must confirm:** that upstream v0.72.120's top-level `CMakeLists.txt` honours these four `WICKED_*`/`USE_AVX*` names with those exact spellings, and that its generator string is literally `Visual Studio 18 2026`. Also unverified: whether upstream's engine target is named `WickedEngine` and emits `WickedEngine.lib` + `WickedEngine_common.dir/Release/*.obj` (the host CMake links `WickedEngine.lib`, `Jolt.lib`, `LUA.lib`, `Utility.lib`). Resolve all of these by reading `var/vendor/WickedEngine/CMakeLists.txt` after `Prepare-Wicked.ps1` has cloned the pin — not by guessing.

### 2. Export objects/libs into the existing cache layout

Reuse the exact relative paths `Build.ps1:23` and `Prepare-Wicked.ps1:36-44` already expect, writing into `var/vendor/wicked-release-cache/`:

| From build dir | To cache dir |
|---|---|
| `WickedEngine/WickedEngine_common.dir/Release/*.obj` | `objects/<name>.obj` |
| `WickedEngine/Release/WickedEngine.lib` | `libraries/WickedEngine.lib` |
| `WickedEngine/Release/Jolt.lib` | `libraries/Jolt.lib` |
| `WickedEngine/LUA/Release/LUA.lib` | `libraries/LUA.lib` |
| `WickedEngine/Utility/Release/Utility.lib` | `libraries/Utility.lib` |
| `Release/dxcompiler.dll` | `bin/dxcompiler.dll` |

**Hard gate, mirroring `Prepare-Wicked.ps1:37`:** `throw` unless the copied object count is exactly 80. Do not relax this. A fresh build that yields 81 or 79 objects has a different translation-unit set than the verified host link, and must fail loudly rather than silently link a different binary.

**Do not** regenerate `source-CMakeCache.txt` from the new build directory — copy the *new* `CMakeCache.txt` over it so `provenance.json:59`'s `cache_source`/`source_root` stay truthful.

### 3. Provenance

Emit `var/vendor/wicked-release-cache/provenance.json` with the **same schema_version 1 shape** `Build.ps1:20-26` validates, so zero downstream changes are needed:

```json
{
  "schema_version": 1,
  "engine_revision": "<git rev-parse HEAD of var/vendor/WickedEngine>",
  "configuration": "Release",
  "cache_source": "var/build/wicked-engine",
  "source_root": "var/vendor/WickedEngine",
  "artifacts": [ {"path": "...", "sha256": "...", "bytes": N}, ... ],
  "note": "Fresh local compile of pinned Wicked source at this revision"
}
```

Only `cache_source` and `note` differ from the copy path; `note` must *not* claim "not a fresh engine rebuild" — honesty over completeness (AGENTS.md #1). Record the actual `cmake --version` and MSVC toolset version too; they are new provenance the copy path never captured and they are the main reproducibility risk.

Then add a marker file (e.g. `provenance.json` field `build: "fresh"`) so an operator can tell a fresh compile from a copied cache at a glance.

### 4. Failure behaviour

Follow `Prepare-Wicked.ps1`'s existing idiom — `$ErrorActionPreference = 'Stop'`, check `$LASTEXITCODE` after every native call, `throw` with the failing path in the message. Concretely:

- Non-zero configure exit → `throw 'Wicked engine configure failed; see var/build/wicked-engine/CMakeCache.txt'`.
- Non-zero build exit → `throw "Wicked engine Release build failed; see $log"`.
- Any of the six expected outputs missing → `throw "Missing engine artifact: <path>"`.
- Object count != 80 → `throw "Fresh build produced $($n) common objects; host requires exactly 80"`.
- SHA-256 of each copied file re-verified *after* copy, exactly as `Prepare-Wicked.ps1:55` does.
- **On any failure, do not write `provenance.json`** — leave the previous verified receipt in place so `Build.ps1` keeps consuming the last known-good cache instead of a half-written one.

### 5. Interaction with the existing path

`Build.ps1` needs **zero changes**: after `Build-Wicked.ps1` succeeds, `Build.ps1` re-verifies revision, `configuration == 'Release'`, the 85-entry unique set, and every SHA-256 — all of which a fresh compile satisfies identically. `Prepare-Wicked.ps1` also needs no change; its `-ReleaseCache` branch remains the "I already have a verified build" path.

One coupling to fix while in there: `Build.ps1:39` writes `cache_provenance_sha256` and the note `'Application built locally against verified copied engine Release inputs'`. That note becomes false after a fresh compile. Add a `-CacheNote` parameter (defaulting to today's string) that `Build-Wicked.ps1` sets to the fresh-compile wording — or simpler, have `Build.ps1` read the `note` from `provenance.json` instead of hardcoding it.

## Suggested invocation

```powershell
./stalthos-runtime/scripts/Prepare-Wicked.ps1                    # source-only is now sufficient
./stalthos-runtime/scripts/Build-Wicked.ps1                       # NEW: configure + build + export + provenance
./stalthos-runtime/scripts/Build.ps1                              # unchanged; consumes the fresh cache
```

## Upstream facts requiring verification

1. Exact option names/spellings upstream v0.72.120 accepts (`WICKED_EMBED_SHADERS`, `WICKED_DYNAMIC_LIBRARY`, `USE_AVX`, `USE_AVX2`) — inferred from the cache gate, not from upstream source.
2. Whether the upstream root `CMakeLists.txt` builds all four static libs (`WickedEngine`, `Jolt`, `LUA`, `Utility`) in a single `cmake --build` invocation, or whether `LUA`/`Utility` are separate subprojects needing extra `--target` args.
3. Whether `dxcompiler.dll` is produced by the build or must be fetched (Wicked historically downloads the Microsoft Agility/DXC package; if so this is a network dependency the task forbids and needs a documented local-cache fallback).
4. Whether upstream's generator string is `Visual Studio 18 2026` (VS2026) or still `Visual Studio 17 2022`, and whether its CMake floor accepts the installed CMake. A generator/CMake mismatch would be the most likely first failure.
5. Whether upstream requires a pre-build `git submodule update --init` step; `Prepare-Wicked.ps1:12-17` uses `git clone` with no `--recurse-submodules`, so any submodule content would already be missing from the source-only path today.
6. Whether the fresh object count is still exactly 80 at this pin — the whole host contract (`CMakeLists.txt:21`, `Build.ps1:24`) hard-codes it, so this should be confirmed empirically on first fresh build rather than assumed.
7. Whether the host's `/arch:AVX` plus `JPH_USE_*`/`_XM_*` definitions (`CMakeLists.txt:30-34`) require the engine to have been compiled with matching `JPH_PROFILE`/Jolt option macros — a mismatch would link but crash at runtime, not at build time, so it needs an explicit runtime smoke test (`Run.ps1 -Duration 12`).

Not verified and not verifiable without running tools: whether `var/vendor/WickedEngine` and `var/vendor/wicked-release-cache` currently exist on this host, and whether the local mirror path in `README.md:23` still resolves.