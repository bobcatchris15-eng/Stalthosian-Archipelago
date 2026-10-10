# Stalthos Toolkit — Operational Pipeline Runbook

**Status:** *Proposed* (v0.1.0-draft). This document maps the canonical 9-stage pipeline from
`Rauken-Archipelago/docs/asset-toolkit.md` onto the 12 declared CLI commands, describing
inputs, outputs, artifact locations, failure modes, and inter-stage checks for an
unattended Windows agent.

> **Grounding rule:** `stalthos list` output is the source of truth for implementation
> status. As of the current snapshot, **all 12 commands are declared but unimplemented
> (exit 5)**. This runbook documents the *intended* contract; every stage marked
> *Not Implemented* must be treated as a hard blocker until the owning worker delivers
> a working implementation.

---

## 1. Canonical 9-Stage Pipeline → 12-Command Mapping

| Stage | Name | Commands | Status |
|-------|------|----------|--------|
| 1 | Source/Research | `research_species` | Not Implemented (exit 5) |
| 2 | Parameterization | *(definition authoring — no CLI command)* | Manual/authored |
| 3 | High-Resolution Generation | `generate_plant`, `build_creature`, `rig_creature` | Not Implemented (exit 5) |
| 4 | Texture/Material Creation | `bake_materials` | Not Implemented (exit 5) |
| 5 | Variants/Stages | `generate_plant_variants` | Not Implemented (exit 5) |
| 6 | Optimization | `generate_lods` | Not Implemented (exit 5) |
| 7 | Interchange | *(handled by `publish_asset`)* | Not Implemented (exit 5) |
| 8 | Quality Assurance | `render_turntable`, `render_growth_stages`, `validate_asset`, `benchmark_asset` | Not Implemented (exit 5) |
| 9 | Publish | `publish_asset` | Not Implemented (exit 5) |

> **Key:** "Not Implemented (exit 5)" means the command exists in the registry but its
> handler is the `not_implemented` stub. Running it emits a JSON status envelope with
> `exit_code: 5`, `ok: false`, and a `NotImplementedStage` error. No artifacts are
> written.

---

## 2. Per-Command Reference

### 2.1 `research_species` — Stage 1: Source/Research

**Purpose:** Collect and normalize species references (literature, measurements, photos,
license metadata) into a machine-readable research record that feeds the definition
authoring step.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--definition` (optional YAML/JSON with `references`, `measurements`, `license` fields), `--seed`, `--workspace` |
| **Outputs** | Research record at `var/reports/research/<species_id>.json` with normalized citations, measurement ranges, license clearance flags, confidence tiers |
| **Artifacts** | Role `report`, sha256-hashed |
| **Exit codes** | 0=success, 2=bad definition, 4=missing tool (none required), 5=unimplemented, 6=tool failure, 7=timeout |
| **Inter-stage check** | Human/agent reviews `license` flags (no uncleared proprietary refs); confirms measurement ranges cover all generator parameters |

> *Not Implemented* — will exit 5. When implemented, should accept a loose research
> dossier and emit a structured `ResearchRecord` schema (TBD in `contracts/`).

---

### 2.2 `generate_plant` — Stage 3: High-Resolution Generation (Plants)

**Purpose:** Generate the canonical **source geometry** (Blender `.blend` master) for a
plant species from its YAML definition. This is the editable master; all downstream
artifacts derive from it.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--definition <plant.yaml>`, `--seed` (int or label), `--asset-id` (optional override), `--stage` (growth stage), `--variant` (morphology variant), `--blender <path>`, `--timeout`, `--dry-run`, `--workspace` |
| **Outputs** | `var/sources/plants/<asset_id>.blend` (editable master), `var/reports/manifests/<asset_id>.json` (provenance manifest) |
| **Artifacts** | Role `source` (the `.blend`), role `report` (manifest) |
| **Determinism** | Root seed → `derive_seed(root, "plant", species_id, stage, variant)` passed to Blender script; all Geometry Nodes / particle systems seeded explicitly |
| **Exit codes** | 0=success, 2=bad definition/missing required fields, 3=validation fail (post-gen), 4=Blender missing, 5=unimplemented, 6=Blender crashed/non-zero, 7=timeout |
| **Inter-stage check** | Agent verifies: `.blend` exists and is loadable; manifest `content_hash` matches expectation; `metrics.triangles` > 0; no orphan objects; all generator parameters traceable to definition |

> *Not Implemented* — will exit 5. When implemented, the Blender script lives in
> `src/stalthos_toolkit/blender/generate_plant.py` and is invoked headless.

---

### 2.3 `build_creature` — Stage 3: High-Resolution Generation (Creatures)

**Purpose:** Build the anatomical proxy geometry (skeleton, base mesh, osteoderm/quill
placement maps) for a creature from its YAML definition. Produces the editable
`.blend` master.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--definition <creature.yaml>`, `--seed`, `--asset-id`, `--stage` (life stage), `--variant`, `--blender`, `--timeout`, `--dry-run`, `--workspace` |
| **Outputs** | `var/sources/creatures/<asset_id>.blend`, `var/reports/manifests/<asset_id>.json` |
| **Artifacts** | Role `source` (`.blend`), role `report` (manifest) |
| **Determinism** | Seed derivation: `derive_seed(root, "creature", species_id, stage, variant)` |
| **Exit codes** | Same as `generate_plant` |
| **Inter-stage check** | Skeleton hierarchy valid; bone names match `BeastCore` convention; osteoderm/quill maps present as vertex groups or geometry nodes; manifest records `growth_curves` per body region |

> *Not Implemented* — will exit 5.

---

### 2.4 `rig_creature` — Stage 3: High-Resolution Generation (Rig)

**Purpose:** Create or refresh the authored rig (IK/FK, drivers, shape keys) on an
existing creature source `.blend` and export neutral poses.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--definition <creature.yaml>` (or `--asset-id` to locate existing source), `--seed`, `--blender`, `--timeout`, `--dry-run`, `--workspace` |
| **Outputs** | Updated `var/sources/creatures/<asset_id>.blend` (rig added), neutral pose exports at `var/intermediate/creatures/<asset_id>/poses/*.glb`, manifest updated |
| **Artifacts** | Role `source` (rigged `.blend`), role `intermediate` (pose GLBs), role `report` (manifest) |
| **Exit codes** | 0=success, 2=missing source/definition, 3=rig validation fail, 4=Blender missing, 5=unimplemented, 6=Blender failure, 7=timeout |
| **Inter-stage check** | Rig hierarchy intact; no cycle in constraints; neutral poses export cleanly; bone roll consistent; vertex groups non-empty for all deform bones |

> *Not Implemented* — will exit 5.

---

### 2.5 `bake_materials` — Stage 4: Texture/Material Creation

**Purpose:** Bake multiscale PBR texture sets (albedo, roughness, normal, thickness/
transmission, AO, curvature, cavity) from the source `.blend` using Blender's baker.
Produces uncompressed intermediate textures; compression happens in `generate_lods`.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--definition` (or `--asset-id`), `--seed`, `--stage`, `--variant`, `--blender`, `--timeout`, `--dry-run`, `--workspace` |
| **Outputs** | `var/intermediate/<kind>/<asset_id>/textures/` (PNG/EXR, 16-bit where appropriate), updated manifest with texture list and UV layout hash |
| **Artifacts** | Role `intermediate` (baked textures), role `report` (manifest) |
| **Determinism** | Blender baker seeded; UV layout fixed by source; no viewport-dependent sampling |
| **Exit codes** | 0=success, 2=bad UVs/missing materials, 3=bake validation fail (seams, padding), 4=Blender/Compressonator missing, 5=unimplemented, 6=Blender failure, 7=timeout |
| **Inter-stage check** | All expected texture slots present; no solid-color fills (indicates bake failure); padding ≥ 16px; normal map in OpenGL convention (documented in manifest); `measured=False` for any estimated metrics |

> *Not Implemented* — will exit 5. **Compressonator** required for BC5/BC7 in publish;
> absence → exit 4 unless `--allow-degraded`.

---

### 2.6 `generate_plant_variants` — Stage 5: Variants/Stages

**Purpose:** Generate seeded morphological variants and growth-stage instances from a
canonical plant source. Each variant is a distinct `.blend` (or lightweight override)
with its own manifest linking back to the parent.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--definition <plant.yaml>`, `--seed`, `--asset-id` (parent), `--count` (number of variants), `--stage`, `--variant`, `--blender`, `--timeout`, `--dry-run`, `--workspace` |
| **Outputs** | `var/sources/plants/<asset_id>_v<NNN>.blend` (or geometry-nodes override JSON), `var/reports/manifests/<asset_id>_v<NNN>.json` |
| **Artifacts** | Role `source` (variant masters), role `report` (manifests) |
| **Determinism** | `derive_seed(root, "variant", parent_asset_id, index)` — each variant fully reproducible |
| **Exit codes** | 0=success, 2=bad count/parent missing, 3=variant validation fail, 4=Blender missing, 5=unimplemented, 6=Blender failure, 7=timeout |
| **Inter-stage check** | Variant count matches `--count`; each manifest `provenance.parent_asset_id` set; silhouettes visually distinct (manual/agent spot-check); no duplicate seeds |

> *Not Implemented* — will exit 5.

---

### 2.7 `generate_lods` — Stage 6: Optimization

**Purpose:** Produce LOD chain (LOD0–LOD3), collision proxy, and streaming cost metrics
from source/intermediate assets. Uses `gltfpack`/`meshoptimizer` when available; falls
back to Blender Decimate with `--allow-degraded`.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--asset-id`, `--definition` (for metadata), `--seed`, `--blender`, `--timeout`, `--allow-degraded`, `--dry-run`, `--workspace` |
| **Outputs** | `var/runtime/<kind>/<asset_id>/lod0.glb`, `lod1.glb`, `lod2.glb`, `lod3.glb`, `collision.glb`, `var/reports/manifests/<asset_id>.json` (updated with LOD metrics) |
| **Artifacts** | Role `runtime` (GLBs), role `report` (manifest) |
| **Determinism** | `gltfpack` seeded; Blender Decimate ratio fixed per LOD level |
| **Exit codes** | 0=success, 2=missing source, 3=LOD validation fail (silhouette drift > threshold), 4=gltfpack/meshoptimizer missing (degraded mode), 5=unimplemented, 6=optimizer failure, 7=timeout |
| **Inter-stage check** | Triangle counts per LOD within budget; silhouette comparison (render diff) < 2% pixel difference at target screen coverage; collision proxy convex hull valid; manifest records `measured=True` for triangle counts |

> *Not Implemented* — will exit 5. **gltfpack** or **meshoptimizer** strongly recommended;
> without them, only degraded Blender decimate is available.

---

### 2.8 `render_turntable` — Stage 8: Quality Assurance (Turntable)

**Purpose:** Render a deterministic turntable review sheet (fixed camera angles, neutral
lighting, fixed resolution) for visual QA and regression detection.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--asset-id`, `--definition`, `--seed`, `--stage`, `--variant`, `--blender`, `--timeout`, `--dry-run`, `--workspace` |
| **Outputs** | `var/preview/<kind>/<asset_id>/turntable/` (PNG frames + contact sheet), manifest updated with preview paths |
| **Artifacts** | Role `preview` (images), role `report` (manifest) |
| **Determinism** | Camera orbit fixed (e.g., 24 frames @ 15° increments, 30° elevation), HDRI fixed, render seed fixed, resolution fixed (1920×1080) |
| **Exit codes** | 0=success, 2=missing runtime asset, 3=render validation fail (black frames, NaN pixels), 4=Blender missing, 5=unimplemented, 6=Blender render failure, 7=timeout |
| **Inter-stage check** | Frame count matches spec; no pure-black frames; manifest `previews` list populated; agent can diff against golden master (perceptual hash) |

> *Not Implemented* — will exit 5.

---

### 2.9 `render_growth_stages` — Stage 8: Quality Assurance (Growth Stages)

**Purpose:** Render a side-by-side comparison sheet of all growth/life stages for a
species (plants: seedling→senescent; creatures: hatchling→senescent).

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--definition`, `--seed`, `--asset-id` (base), `--blender`, `--timeout`, `--dry-run`, `--workspace` |
| **Outputs** | `var/preview/<kind>/<asset_id>/growth_stages/` (composite image + individual frames), manifest updated |
| **Artifacts** | Role `preview`, role `report` |
| **Determinism** | Same camera/lighting per stage; stage order fixed by definition |
| **Exit codes** | Same as `render_turntable` |
| **Inter-stage check** | All defined stages rendered; proportions match `growth_curves` in definition; no missing stages |

> *Not Implemented* — will exit 5.

---

### 2.10 `validate_asset` — Stage 8: Quality Assurance (Validation)

**Purpose:** Run automated acceptance checks against the published runtime asset:
geometry, normals, UVs, skeleton, morph targets, surface artifacts, shadow/alpha,
provenance completeness.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--asset-id`, `--definition`, `--strict` (warnings → failures), `--workspace` |
| **Outputs** | `var/reports/validation/<asset_id>.json` (detailed check results), manifest `validation.outcome` updated |
| **Artifacts** | Role `report` |
| **Exit codes** | 0=pass, 2=missing asset/manifest, 3=validation failed (any check), 4=validator tool missing (none required), 5=unimplemented, 6=validator crashed, 7=timeout |
| **Checks (minimum)** | • Mesh: manifold, no degenerate faces, UVs in [0,1], no overlapping UVs (unless intentional) • Normals: consistent orientation, no hard-edge artifacts • Skeleton: hierarchy matches BeastCore, no orphan bones, weights sum to 1 • Materials: all texture slots filled, normal map convention recorded • Provenance: manifest `content_hash` reproducible, all input hashes present • Affordances: browse heights, contact bounds present |
| **Inter-stage check** | `validation.outcome == "pass"` required before `publish_asset`; `--strict` promotes warnings to exit 3 |

> *Not Implemented* — will exit 5.

---

### 2.11 `benchmark_asset` — Stage 8: Quality Assurance (Benchmark)

**Purpose:** Measure GPU/CPU/memory cost of the runtime asset on the reference machine
(GTX 1080 baseline). Reports measured frame-time, VRAM, draw calls, triangle throughput.
Does **not** infer — only reports measured values (`measured=True`).

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--asset-id`, `--definition`, `--workspace`, `--engine` (optional, for engine-specific path) |
| **Outputs** | `var/reports/benchmark/<asset_id>.json`, manifest `metrics` extended with measured profile |
| **Artifacts** | Role `report` |
| **Exit codes** | 0=success, 2=missing runtime asset, 3=benchmark fail (crash, OOM), 4=engine/RenderDoc/PresentMon missing, 5=unimplemented, 6=benchmark tool failure, 7=timeout |
| **Metrics reported** | `triangles_lod0`, `triangles_lod1`, `vr_mb`, `draw_calls`, `frame_ms_p50`, `frame_ms_p99`, `texture_memory_mb`, `shader_variants` |
| **Inter-stage check** | All metrics `measured=True`; frame time < 16.67ms (60 FPS) at target LOD on GTX 1080; VRAM < budget; manifest updated |

> *Not Implemented* — will exit 5. Requires a renderer harness (Flax/Wicked) — not in
> toolkit scope. Toolkit only defines the **output schema** and invocation contract.

---

### 2.12 `publish_asset` — Stage 9: Publish

**Purpose:** Promote validated artifacts from `var/intermediate/` and `var/runtime/`
to the final published location (currently `var/runtime/` is the publish target;
future: copy to engine content directory). Writes a **build receipt** manifest with
`content_hash` for reproducibility verification.

| Aspect | Detail |
|--------|--------|
| **Inputs** | `--asset-id`, `--definition`, `--workspace`, `--strict` |
| **Outputs** | `var/reports/receipts/<asset_id>.json` (build receipt), `var/runtime/` assets confirmed, manifest `validation.outcome` must be `pass` |
| **Artifacts** | Role `runtime` (confirmed), role `report` (receipt) |
| **Exit codes** | 0=published, 2=missing asset/manifest, 3=validation not passed, 4=missing dependency (none), 5=unimplemented, 6=copy/verify failure, 7=timeout |
| **Rules** | • Refuses to promote if `manifest.validation.outcome != "pass"` • Refuses if `partial: true` on any upstream status • Verifies `content_hash` matches re-computed hash • Records tool versions at publish time |
| **Inter-stage check** | Receipt `content_hash` matches manifest; all runtime files present and hashed; license provenance complete |

> *Not Implemented* — will exit 5.

---

## 3. Initial Tool-Building Experiment (Pipeline Verification)

The plan calls for building **six reference plants** and **one sauropod anatomical
proxy** through the full pipeline as a verification exercise.

### 3.1 Reference Species Definitions (already authored)

| Asset ID | Definition File | Stage 3 Command | Stage 5 Variants |
|----------|-----------------|-----------------|------------------|
| `plant.splitcrown` | `species/plants/splitcrown.yaml` | `generate_plant` | `generate_plant_variants --count 5` |
| `plant.softbough` | `species/plants/softbough.yaml` | `generate_plant` | `generate_plant_variants --count 5` |
| `plant.jointreed` | `species/plants/jointreed.yaml` | `generate_plant` | `generate_plant_variants --count 5` |
| `plant.royal_frond` | `species/plants/royal_frond.yaml` | `generate_plant` | `generate_plant_variants --count 5` |
| `plant.basketfern` | `species/plants/basketfern.yaml` | `generate_plant` | `generate_plant_variants --count 5` |
| `plant.bracket_fungus` | `species/plants/bracket_fungus.yaml` | `generate_plant` | `generate_plant_variants --count 3` |
| `creature.smalltrotter` | `species/creatures/smalltrotter.yaml` | `build_creature` + `rig_creature` | (creature variants via `--variant`) |

### 3.2 Full Pipeline Sequence (Per Asset)

```powershell
# For each plant species:
stalthos research_species -d species/plants/splitcrown.yaml
stalthos generate_plant   -d species/plants/splitcrown.yaml -s 42
stalthos bake_materials   -d species/plants/splitcrown.yaml -s 42
stalthos generate_plant_variants -d species/plants/splitcrown.yaml -s 42 --count 5
stalthos generate_lods    -d species/plants/splitcrown.yaml -s 42
stalthos render_turntable -d species/plants/splitcrown.yaml -s 42
stalthos render_growth_stages -d species/plants/splitcrown.yaml -s 42
stalthos validate_asset   -d species/plants/splitcrown.yaml
stalthos benchmark_asset  -d species/plants/splitcrown.yaml
stalthos publish_asset    -d species/plants/splitcrown.yaml

# For the sauropod proxy:
stalthos research_species -d species/creatures/smalltrotter.yaml
stalthos build_creature   -d species/creatures/smalltrotter.yaml -s 42 --stage independent_juvenile
stalthos rig_creature     -d species/creatures/smalltrotter.yaml -s 42 --stage independent_juvenile
stalthos bake_materials   -d species/creatures/smalltrotter.yaml -s 42 --stage independent_juvenile
stalthos generate_lods    -d species/creatures/smalltrotter.yaml -s 42 --stage independent_juvenile
stalthos render_turntable -d species/creatures/smalltrotter.yaml -s 42 --stage independent_juvenile
stalthos render_growth_stages -d species/creatures/smalltrotter.yaml -s 42
stalthos validate_asset   -d species/creatures/smalltrotter.yaml
stalthos benchmark_asset  -d species/creatures/smalltrotter.yaml
stalthos publish_asset    -d species/creatures/smalltrotter.yaml
```

> **Current reality:** Every command above exits 5. The pipeline **cannot run
> end-to-end today**. This section documents the *target* sequence; each stage
> will unblock as its owning worker delivers.

---

## 4. Unattended PowerShell Run Recipe

The `scripts/stalthos.ps1` entrypoint (deliverable #2) implements this recipe.
Usage:

```powershell
# Full pipeline for one asset
.\scripts\stalthos.ps1 -Definition species/plants/splitcrown.yaml -Seed 42 -Workspace .\var

# Dry-run to see planned stages without writing
.\scripts\stalthos.ps1 -Definition species/plants/splitcrown.yaml -Seed 42 -DryRun

# Run only specific stages (comma-separated stage numbers)
.\scripts\stalthos.ps1 -Definition species/plants/splitcrown.yaml -Seed 42 -Stages "3,4,5"
```

### 4.1 Per-Stage JSON Status Capture

The entrypoint captures each stage's stdout (the JSON status envelope) to:
```
var/reports/status/<command>_<asset_id>_<timestamp>.json
```
It also writes a **pipeline summary** at:
```
var/reports/status/pipeline_<asset_id>_<timestamp>.json
```
containing an array of per-stage status objects plus `overall_exit_code` (first
non-zero) and `completed_stages`.

### 4.2 Exit-Code Propagation

- The script stops at the **first non-zero exit code** (unless `-ContinueOnError`).
- The final script exit code = the first failed stage's exit code.
- This makes CI/CD integration trivial: `if ($LASTEXITCODE) { fail }`.

### 4.3 Parameter Reference

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `-Definition` | string | *required* | Path to species/creature YAML |
| `-Seed` | int/string | `0` | Root seed for determinism |
| `-Workspace` | string | `$env:STEALTHOS_HOME` or `.\var` | Workspace root |
| `-Stages` | string | `"1..9"` | Comma-separated stage numbers or ranges |
| `-DryRun` | switch | false | Plan only, no writes |
| `-ContinueOnError` | switch | false | Run all stages, collect all failures |
| `-Transcript` | string | none | Path to full PowerShell transcript log |
| `-BlenderPath` | string | auto-discover | Override Blender executable |

---

## 5. Failure Playbook (Exit Codes)

| Code | Meaning | Operator Action |
|------|---------|-----------------|
| **0** | Success | Proceed to next stage |
| **1** | Unclassified runtime failure | Check `errors[].detail` in status JSON; re-run with higher `--timeout`; if persistent, file bug |
| **2** | Usage error / malformed definition | Fix definition YAML (schema validation); check `--definition` path; run `stalthos list` for valid commands |
| **3** | Validation/acceptance failure | Inspect `var/reports/validation/<asset_id>.json`; fix geometry/material/rig issues in source `.blend`; re-run from failed stage |
| **4** | Required external tool absent | Install missing tool (Blender, gltfpack, Compressonator, etc.); ensure on PATH or set `STEALTHOS_BLENDER` / `--blender`; re-run |
| **5** | Command declared but not implemented | **Hard blocker** — the stage's owning worker must implement it. Check `stalthos list` for `---` marker. Cannot proceed. |
| **6** | External tool ran and failed | Check `errors[].detail.stderr` (last 4KB); common: Blender OOM, gltfpack invalid mesh, Compressonator format error. Fix input or tool config. |
| **7** | Timeout or operator interrupt | Increase `--timeout`; if Blender hang, check scene complexity; Ctrl+C sends this code |

> **Critical:** Exit 5 is *not* a transient error. It means the pipeline stage does not
> exist in code. No amount of re-running or config fixes will unblock it.

---

## 6. Determinism & Reproducibility Rules (Agent Must Follow)

1. **Root seed is the single source of stochasticity.** Always pass `--seed` (int or
   stable string label). Never omit it for production runs.
2. **Seed derivation is hierarchical.** `derive_seed(root, "stage", "species", "variant", index)`
   — the label path is part of the contract. Do not invent ad-hoc labels.
3. **No global RNG.** Never use `random.random()`, `numpy.random`, `hash()`, or
   Blender's internal RNG without explicit seeding from the derived seed.
4. **Tool versions are part of provenance.** The manifest records `tool_versions`
   (Blender, gltfpack, Compressonator). A tool upgrade changes the manifest but
   `content_hash` ignores it — use `content_hash` to compare semantic equivalence.
5. **Input hashes are recorded.** `manifest.provenance.input_hashes` includes the
   definition file sha256 and `hash_config(definition)`. Changing the definition
   changes the hash → new asset.
6. **Workspace isolation.** All writes under `var/`. No absolute paths outside
   workspace. `Workspace.path_for()` refuses escapes (raises `ValueError`).
7. **Manifest `content_hash` is the reproducibility certificate.** Two runs with
   identical (definition, seed, tool versions) must produce identical
   `content_hash`. If they differ, the pipeline is non-deterministic — treat as bug.

---

## 7. Adding a New Species (Breadth Rule)

The plan's **breadth rule**: *adding a favorite creature or plant should normally
entail adding a profile/assets and habitat tags, not modifying SimCore, FaunaSim,
or FloraSim.*

### 7.1 New Plant Species

1. Create `species/plants/<new_species>.yaml` with:
   - `id`, `morphology_id`, `kind: "plant"`
   - `habitat_tags` (for FloraSim placement)
   - `growth_stages`, `variants`
   - `mature_dimensions_m`, `edible_parts`, `defenses`, `affordances`
   - `generator` (Geometry Nodes group name) + `parameters`
   - `references[]`, `licenses[]`
2. Run `stalthos research_species -d species/plants/<new_species>.yaml` (when implemented)
3. Run the full pipeline (stages 3–9)
4. **No Python/Blender code changes required** if the `generator` exists.
5. If a new morphology generator is needed, add it to `blender/` Geometry Nodes
   library — still **no SimCore/FloraSim changes**.

### 7.2 New Creature Species

1. Create `species/creatures/<new_creature>.yaml` with:
   - `id`, `morphology_id`, `kind: "creature"`
   - `clade`, `life_stages`, `variants`
   - `physical_envelope_m` per stage
   - `growth_curves` per body region
   - `anatomy` (skeleton counts, integument zones, musculature)
   - `behavior` (diet, browse_envelope, sociality)
   - `affordances` (contact_points, gait_cycles, interaction_anchors)
   - `generator` + `parameters`
2. Run pipeline (stages 3–9, using `build_creature` + `rig_creature`)
3. **No BeastCore/HeroSim/FaunaSim changes** — behavior parameters are data.

### 7.3 Habitat Tags

Both plants and creatures use `habitat_tags` (plants) / ecology fields (creatures)
that FaunaSim/FloraSim read at runtime. The simulation systems are **data-driven**;
new tags are recognized by the simulation without code changes, provided the tag
vocabulary is shared (see `Rauken-Archipelago/docs/ecology-and-hydrology.md`).

---

## 8. Workspace Layout (Runtime)

```
var/
  sources/
    plants/           # .blend masters (LFS)
    creatures/        # .blend masters (LFS)
  intermediate/
    plants/<asset_id>/
      textures/       # baked PNG/EXR
      variants/       # variant overrides
    creatures/<asset_id>/
      textures/
      poses/          # neutral pose GLBs
  runtime/
    plants/<asset_id>/
      lod0.glb .. lod3.glb
      collision.glb
      textures/       # BC5/BC7 KTX2
    creatures/<asset_id>/...
  preview/
    plants/<asset_id>/
      turntable/
      growth_stages/
    creatures/<asset_id>/...
  reports/
    manifests/        # per-asset provenance
    validation/       # validate_asset output
    benchmark/        # benchmark_asset output
    receipts/         # publish_asset receipts
    status/           # per-stage JSON status (from stalthos.ps1)
    research/         # research_species output
```

---

## 9. Contract Problems & Open Questions (For Operator)

### 9.1 Core/Contract Issues Found

| Issue | Severity | Description |
|-------|----------|-------------|
| **Missing `contracts/` schemas** | High | `docs/toolkit-architecture.md` references `contracts/` JSON schemas for validation, but the directory doesn't exist. `schemas/loader.py` has no schema validation — it only parses YAML/JSON/TOML. |
| **`research_species` has no output schema** | Medium | The command is declared but no `ResearchRecord` schema exists in `contracts/`. The runbook assumes one. |
| **`generate_plant_variants` output model unclear** | Medium | Does it produce full `.blend` files per variant, or lightweight Geometry Nodes override JSONs? The architecture says "source masters stay editable" — full `.blend` per variant may bloat LFS. |
| **`benchmark_asset` requires external renderer** | High | The toolkit defines the *output schema* but not the harness. Need a decision: minimal headless Blender render benchmark vs. engine-specific (Flax/Wicked). |
| **No `publish_asset` destination config** | Low | Currently publishes to `var/runtime/`. Real deploy needs engine content path. Should be configurable. |
| **`stalthos doctor` doesn't check Python package versions** | Low | Only checks external CLIs. Could record `stalthos-toolkit`, `PyYAML`, `blender --version` parse. |

### 9.2 Open Questions for Operator

1. **Variant storage model:** Full `.blend` per variant vs. Geometry Nodes override JSON? Impacts LFS cost and `generate_plant_variants` implementation.
2. **Benchmark harness:** Accept headless Blender Eevee/Cycles render time as proxy, or require Flax/Wicked integration?
3. **Schema validation:** Should `schemas/loader.py` validate against JSON Schema (require `jsonschema` dep) or keep schema-less?
4. **Creature rig standard:** `BeastCore` bone naming convention needs to be documented in `contracts/` so `rig_creature` and `validate_asset` agree.
5. **License provenance:** `references[]` and `licenses[]` in definitions — should the toolkit enforce a controlled vocabulary (CC0, CC-BY, proprietary, etc.)?

---

## 10. Status Legend

| Marker | Meaning |
|--------|---------|
| *Decided* | Agreed, implemented or documented as frozen contract |
| *Proposed* | Documented here, awaiting implementation or review |
| *To test* | Behavior described but not verified against running code |
| *Unknown* | Genuinely unclear — needs design decision |
| *Not Implemented* | Command exists in registry but handler is stub (exit 5) |

---

*Generated by PipelineScribe (W06). Source of truth: `stalthos list` output and
`docs/toolkit-architecture.md`. This runbook must be updated by the owning worker
(D) as stages are implemented.*