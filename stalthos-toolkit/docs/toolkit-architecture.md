# Stalthos Asset Toolkit — architecture and command contract

**Status:** *Decided* (foundation layer, v0.1.0). Implements the production tooling
plan in `Rauken-Archipelago/docs/asset-toolkit.md`. That repository stays
planning-only; this repository is code.

**Non-negotiables inherited from the plan**
- Windows-first, unattended, PowerShell entrypoint.
- Free/open source only. No paid recurring API rates, no hard dependency on a
  proprietary tool. Optional tools degrade to a typed `MissingDependency`.
- Reproducible: same (definition, seed, tool versions) ⇒ same bytes out.
- Source masters stay editable; runtime assets are derived, never hand-edited.
- Display fidelity beats internal fidelity; expensive mechanisms are opt-in.

---

## 1. Repository layout

```
stalthos-toolkit/
  pyproject.toml            # stdlib + PyYAML + Pydantic 2; console script `stalthos`
  contracts/                # JSON Schemas (the machine-readable contract)
  docs/
    toolkit-architecture.md # this file: contracts, invariants, ownership
    toolkit-pipeline.md     # operational runbook: stage -> command -> artifacts
  scripts/stalthos.ps1      # Windows entrypoint (the unattended entry)
  species/
    plants/*.yaml           # ~6 reference plants (pipeline verification)
    creatures/*.yaml        # sauropod anatomical proxy + roster stubs
  src/stalthos_toolkit/
    core/                   # FOUNDATION - owned by worker zero, treat as API
      errors.py             # exit codes + exception hierarchy
      status.py             # the JSON status envelope
      seeding.py            # deterministic seed derivation + hashing
      workspace.py          # workspace layout + external tool discovery
      manifest.py           # provenance record
      contract.py           # Context, CommandSpec, Registry, @command
    commands/               # one module per CLI command (stage implementations)
    blender/                # headless Blender runner + bpy scripts
    optimize/               # LOD, texture compression, glTF export
    validate/               # acceptance checks
    benchmark/              # measurement harness
    schemas/                # definition loading + schema helpers
  tests/                    # pytest; no Blender required for the core tests
```

`var/` (the workspace root, git-ignored except `.gitkeep`) holds all output:
`sources/ intermediate/ runtime/ preview/ reports/`. Nothing is written outside it.

---

## 2. Command contract

Declared commands (the plan's list, verbatim):

```
research_species        stage 1  source/research
generate_plant          stage 3  high-resolution generation
generate_plant_variants stage 5  variants/stages
build_creature          stage 3  high-resolution generation
rig_creature            stage 3  high-resolution generation
bake_materials          stage 4  texture/material creation
generate_lods           stage 6  optimization
render_turntable        stage 8  quality assurance
render_growth_stages    stage 8  quality assurance
validate_asset          stage 8  quality assurance
benchmark_asset         stage 8  quality assurance
publish_asset           stage 9  publish
```

An **unimplemented command exits 5 with a typed error**. It never reports
success and never writes a runtime artifact. `stalthos list` shows the real
status; the table above is not a claim.

### 2.1 Invocation

```
stalthos <command> [-d DEFINITION] [--seed N|LABEL] [--asset-id ID]
                 [--stage STAGE] [--variant NAME] [--count N] [--out PATH]
                 [--blender PATH] [--timeout SECONDS] [--strict] [--dry-run]
                 [--workspace ROOT] [--quiet]
```

Global: `--workspace/-w` (default `$STEALTHOS_HOME`, else `./var`), `--seed/-s`
(int or stable string label), `--quiet/-q`.

### 2.2 Output — exactly one JSON object on stdout

```json
{
  "command": "generate_plant",
  "ok": true,
  "exit_code": 0,
  "seed": 42,
  "asset_id": "plant.araucaria.sp1",
  "started_at": 1757500000.0,
  "duration_s": 3.21,
  "partial": false,
  "artifacts": [{"path": "...", "role": "source", "sha256": "...", "bytes": 1234}],
  "metrics": {"triangles": 48213, "materials": 2},
  "provenance": {"generator": "stalthos-toolkit", "tool_versions": {"blender": "5.2"}},
  "warnings": [],
  "errors": [],
  "logs": []
}
```

Rules:
- **stdout carries the status envelope and nothing else.** Logs → stderr / `--quiet`.
- `ok` mirrors the exit code. `partial: true` means intermediate files exist but
  nothing was published; `publish_asset` must refuse to promote a partial asset.
- Every produced file is listed as an artifact with a role and a sha256.
- A failure still emits the envelope. Never raise a traceback on stdout.

### 2.3 Exit codes

| Code | Meaning |
|---|---|
0 | success |
1 | unclassified runtime failure |
2 | usage error / malformed definition |
3 | validation or acceptance failure |
4 | required external tool absent |
5 | command declared but not implemented |
6 | external tool ran and failed |
7 | timeout or operator interrupt |

---

## 3. Determinism

```python
from stalthos_toolkit import Context, derive_seed, rng

ctx.seed_for("plant", species_id, "branch", index)   # stable child seed
r = ctx.rng("plant", species_id, "crown")            # random.Random, seeded
```

- Every stochastic decision derives from the root seed through
  `derive_seed(root, *labels)` (sha256 over a `|`-joined label path).
- **Never** use the global `random`, `numpy.random` without a generator, or
  `hash()` — they are not stable across runs/processes.
- Blender-side randomness must be seeded explicitly from the derived seed that
  the Python side passes in; never let bpy pick its own seed.
- The manifest records the root seed plus `input_hashes`, so a run can be proven
  reproducible without trusting the filesystem.

---

## 4. Provenance manifest

One manifest per asset, written to `var/reports/manifests/<asset_id>.json`.
Required content (from the plan's "Data and provenance" section):

- stable logical id, species/morphology id, source version, seed, stage, variant
- tool versions (blender, gltfpack, …), input hashes, licensed references,
  source ↔ baked file relationships
- anatomical/growth parameters and ecological affordance metadata
  (`affordances`, e.g. edible foliage targets, plant contact bounds)
- mesh/texture/memory/LOD counts and **measured** benchmark profile
  (`Metric.measured = False` marks an estimate; never silently infer)
- validation outcome, preview images, automation logs
- license provenance and redistribution notes

`Manifest.content_hash()` ignores timestamps and tool versions so two runs can
be compared semantically.

---

## 5. Stage ownership (who may edit what)

`core/` is the frozen API for the dispatched workers. Stage workers edit only
their own modules. If a stage genuinely needs a new capability in `core/`, it
must implement it locally and report the proposed core change in its result —
do not edit `core/` unilaterally.

| Area | Owning stage | Modules |
|---|---|---|
Foundation | worker zero (frozen) | `core/`, `cli.py`, `schemas/loader.py`, `docs/toolkit-architecture.md` |
Schemas + definitions | worker A | `schemas/`, `contracts/`, `species/`, `validate/` |
Blender execution | worker B | `blender/` + `commands/generate_plant.py`, `generate_plant_variants.py`, `build_creature.py`, `rig_creature.py`, `bake_materials.py`, `render_*.py` |
Optimize / publish | worker C | `optimize/`, `commands/generate_lods.py`, `publish_asset.py`, `benchmark_asset.py` |
Validation | worker A | `commands/validate_asset.py`, `research_species.py` |
Docs + entrypoint | worker D | `docs/toolkit-pipeline.md`, `scripts/`, `README.md`, `tests/` (smoke) |

Cross-worker merges are resolved by worker zero after review.

---

## 6. External tools

Discovery lives in `core/workspace.py` (`find_blender`, `find_tool`).
Blender is discovered via `--blender`, then `$STEALTHOS_BLENDER`, then the
standard install paths.

| Tool | Role | If missing |
|---|---|---|
Blender 4.3+ | all generation, baking, rendering | exit 4 with install hint |
`gltfpack` / `meshoptimizer` | LOD + glTF optimization | exit 4, or fall back to Blender's own decimate/simplify when `--allow-degraded` |
AMD Compressonator | BC5/BC7 texture compression | exit 4; uncompressed textures are never published silently — warn + record `measured=False` |
`ktx` / KTX-Software | basis/KTX2 textures | exit 4 |
ImageMagick / FFmpeg | review sheet assembly, render diffs | exit 4 |
RenderDoc / PresentMon / Tracy | frame-time validation | *not* required by the toolkit; consumed by `benchmark_asset` output if present |

`stalthos doctor` prints availability as JSON. That is the supported way for an
agent to check the machine before planning work.

---

## 7. Testing

- `tests/` must pass **without Blender** — core tests only (seeding, manifest,
  status/exit codes, schema validation, CLI plumbing). Use a fake/missing-tool
  path in tests.
- Tests assert on the JSON status envelope and exit codes, never on log text.
- Anything that needs Blender is marked and skipped when Blender is absent.
