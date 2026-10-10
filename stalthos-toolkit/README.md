# Stalthos Asset Toolkit

**Agent-accessible, reproducible asset production pipeline for The Stalthosian Archipelago.**

This repository contains the **implementation** (code, CLI, tests, operational docs).
The **planning documents** live in the sibling repository `Rauken-Archipelago/` —
that repository is **planning-only and must not receive code changes**.

---

## What This Repo Is

A Windows-first, free/open-source toolkit that lets an agent (or human) drive the
full asset pipeline from a species definition YAML to a validated, benchmarked,
published runtime asset — unattended, deterministically, with full provenance.

- **Console script:** `stalthos` (installed via `pip install -e .`)
- **Entrypoint:** `scripts/stalthos.ps1` (PowerShell 5.1 / pwsh 7 compatible)
- **Core language:** Python 3.11+ (stdlib + PyYAML only)
- **External tools:** Blender 4.3+ (required), gltfpack/meshoptimizer, AMD Compressonator (optional but recommended)
- **Output:** JSON status envelopes on stdout, artifacts under `var/`, full provenance manifests

---

## Quick Start (Windows)

```powershell
# 1. Clone the monorepo (this repo is at stalthos-toolkit/)
git clone <monorepo-url>
cd Stalthosian-Archipelago/stalthos-toolkit

# 2. Install in editable mode (adds `stalthos` to PATH)
pip install -e .

# 3. Verify installation (60-second smoke test)
stalthos list
stalthos doctor

# 4. Run the pipeline on a reference plant (will exit 5 until stages are implemented)
.\scripts\stalthos.ps1 -Definition species/plants/splitcrown.yaml -Seed 42
```

### Expected Smoke Test Output

```text
> stalthos list
--- research_species         [1 source/research]  Collect and normalize species references...
--- generate_plant           [3 high-resolution generation]  Generate canonical plant source geometry...
--- generate_plant_variants  [5 variants/stages]  Generate seeded morphological variants...
--- build_creature           [3 high-resolution generation]  Build creature anatomical proxy geometry...
--- rig_creature             [3 high-resolution generation]  Create/refresh the authored rig...
--- bake_materials           [4 texture/material creation]  Bake multiscale material structure...
--- generate_lods            [6 optimization]  Generate LOD chain, collision proxy...
--- render_turntable         [8 quality assurance]  Render a deterministic turntable review sheet...
--- render_growth_stages     [8 quality assurance]  Render a growth/life stage comparison sheet...
--- validate_asset           [8 quality assurance]  Run geometry/material/rig/provenance checks...
--- benchmark_asset          [8 quality assurance]  Measure geometry, texture, memory and frame-time cost...
--- publish_asset            [9 publish]  Promote validated artifacts to runtime/...

> stalthos doctor
{
  "command": "doctor",
  "ok": true,
  "exit_code": 0,
  "metrics": {
    "tools": {
      "blender": {"available": true, "path": "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe"},
      "gltfpack": {"available": false, "path": null},
      ...
    },
    "workspace": "...\\stalthos-toolkit\\var",
    "python": "3.14.5"
  }
}
```

> **Note:** All 12 commands currently show `---` (unimplemented, exit 5). This is the
> *current reality* — the pipeline is a contract waiting for stage workers to implement.
> See `docs/toolkit-pipeline.md` for the stage→command map and implementation status.

---

## Workspace Layout

All toolkit output lives under the **workspace root** (`$env:STEALTHOS_HOME` or `./var`):

```
var/
  sources/           # Editable Blender masters (.blend) — LFS territory
    plants/
    creatures/
  intermediate/      # Bakes, LOD intermediates, scratch renders, neutral poses
    plants/<asset_id>/
      textures/
      variants/
    creatures/<asset_id>/
      textures/
      poses/
  runtime/           # Published GLB + compressed textures (engine-ready)
    plants/<asset_id>/
      lod0.glb .. lod3.glb
      collision.glb
      textures/      # BC5/BC7 KTX2
  preview/           # Turntables, growth stages, review sheets
    plants/<asset_id>/
      turntable/
      growth_stages/
  reports/           # Validation, benchmark, receipts, status JSON, research
    manifests/       # Per-asset provenance (Manifest schema)
    validation/
    benchmark/
    receipts/
    status/          # Per-stage JSON status (from stalthos.ps1)
    research/
```

> **Invariant:** Nothing is written outside the workspace root. `Workspace.path_for()`
> refuses path escapes.

---

## Documentation

| Document | Purpose |
|----------|---------|
| `docs/toolkit-architecture.md` | **Frozen contract**: command registry, JSON status envelope, exit codes, determinism rules, manifest schema, workspace layout, external tool policy. **Read this first.** |
| `docs/toolkit-pipeline.md` | **Operational runbook**: stage→command mapping, per-command I/O, artifact locations, failure playbook, unattended PowerShell recipe, determinism rules, how to add new species. |

---

## Adding a New Species (Breadth Rule)

The plan's breadth rule: *adding a plant or creature should entail adding a YAML profile
and habitat tags, NOT modifying SimCore/FloraSim/FaunaSim.*

### New Plant
1. Create `species/plants/<name>.yaml` with: `id`, `morphology_id`, `habitat_tags`,
   `growth_stages`, `variants`, `edible_parts`, `affordances`, `generator` + `parameters`,
   `references[]`, `licenses[]`.
2. Run pipeline: `.\scripts\stalthos.ps1 -Definition species/plants/<name>.yaml -Seed 42`
3. **No Python/Blender code changes** if the `generator` (Geometry Nodes group) exists.

### New Creature
1. Create `species/creatures/<name>.yaml` with: `id`, `morphology_id`, `life_stages`,
   `physical_envelope_m`, `growth_curves`, `anatomy`, `behavior`, `affordances`,
   `generator` + `parameters`.
2. Run pipeline (uses `build_creature` + `rig_creature`).
3. **No BeastCore/HeroSim/FaunaSim changes** — behavior parameters are data.

Habitat tags are shared vocabulary read by FaunaSim/FloraSim at runtime (see
`Rauken-Archipelago/docs/ecology-and-hydrology.md`).

---

## Reference Species (Pipeline Verification)

The plan defines six reference plants and one sauropod proxy to exercise the pipeline:

| Asset ID | Definition | Description |
|----------|------------|-------------|
| `plant.splitcrown` | `species/plants/splitcrown.yaml` | Araucarian-like large conifer |
| `plant.softbough` | `species/plants/softbough.yaml` | Riparian flexible shrub (key browse) |
| `plant.jointreed` | `species/plants/jointreed.yaml` | Horsetail analogue (segmented stems) |
| `plant.royal_frond` | `species/plants/royal_frond.yaml` | Royal fern (bipinnate fronds) |
| `plant.basketfern` | `species/plants/basketfern.yaml` | Giant marattioid fern |
| `plant.bracket_fungus` | `species/plants/bracket_fungus.yaml` | Polypore on dead wood |
| `creature.smalltrotter` | `species/creatures/smalltrotter.yaml` | Dwarf titanosaur anatomical proxy |

---

## Determinism & Reproducibility

- **Root seed** (`--seed`) is the single source of stochasticity.
- All sub-seeds derived via `derive_seed(root, "stage", "species", "variant", index)` —
  stable across runs, machines, Python versions.
- **Never** use global `random`, `numpy.random`, `hash()`, or unseeded Blender RNG.
- Manifest `content_hash()` ignores timestamps/tool versions — two runs with same
  (definition, seed, tool versions) must produce identical `content_hash`.

---

## Exit Code Contract

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Unclassified runtime failure |
| 2 | Usage error / malformed definition |
| 3 | Validation/acceptance failure |
| 4 | Required external tool absent (Blender, gltfpack, Compressonator...) |
| 5 | Command declared but **not implemented** (hard blocker) |
| 6 | External tool ran and failed |
| 7 | Timeout or operator interrupt |

**Every command emits exactly one JSON object on stdout** (the status envelope),
even on failure. Logs go to stderr.

---

## Testing

```powershell
# Run core tests (no Blender required)
pytest tests/ -v
```

Tests cover: CLI exit codes, JSON envelope shape, deterministic seeding,
manifest round-trip, workspace path-escape refusal. **Assertions are on JSON
envelope and exit codes only — never on log text.**

---

## Relationship to Rauken-Archipelago

```
Stalthosian-Archipelago/
├── Rauken-Archipelago/          # PLANNING ONLY — docs, designs, research
│   └── docs/
│       ├── asset-toolkit.md     # Tooling plan (source of 9-stage pipeline)
│       ├── simulation-architecture.md
│       ├── flora.md, creatures.md, ...
│       └── ...
└── stalthos-toolkit/            # THIS REPO — CODE, CLI, TESTS, OPS DOCS
    ├── src/stalthos_toolkit/
    ├── scripts/stalthos.ps1
    ├── docs/toolkit-architecture.md
    ├── docs/toolkit-pipeline.md
    ├── species/
    └── tests/
```

> **Rule:** `Rauken-Archipelago/` receives **no code changes**. It is the
> planning/specification layer. All implementation lives here.

---

## License

Proprietary to The Stalthosian Archipelago project. See individual asset
`licenses[]` in species definitions for third-party reference provenance.

---

## Status

**v0.1.0-draft** — Foundation contract frozen (`toolkit-architecture.md`),
pipeline runbook drafted (`toolkit-pipeline.md`), PowerShell entrypoint written,
tests scaffolded. All 12 commands are **declared but unimplemented (exit 5)**.
Stage workers (A, B, C) own command implementations; worker D owns this
operational layer.