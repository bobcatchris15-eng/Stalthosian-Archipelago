# AGENTS.md — working in this repository

## What this is

**The Stalthosian Archipelago** — a fictional Late Cretaceous-inspired island
archipelago and small-dinosaur life simulator. Display-first natural-history
illusion: near-photorealistic scenery, convincing animal movement, believable
observable consequences. Target hardware: GTX 1080 8 GB + first-gen Ryzen,
30 FPS floor.

Two halves, do not confuse them:

- `Rauken-Archipelago/` — **planning docs only**. Separate upstream clone
  (`github.com/bobcatchris15-eng/Rauken-Archipelago`), read-only input.
  **Never commit code there.** Its `docs/asset-toolkit.md` is the plan the
  toolkit implements; `docs/README.md` is the index and states the project-wide
  rules.
- `stalthos-toolkit/` — **the code**. Agent-facing, reproducible asset
  production pipeline. Start at `stalthos-toolkit/docs/toolkit-architecture.md`
  (the frozen contract) and `stalthos-toolkit/docs/toolkit-pipeline.md` (the runbook).

## Read before you write

- `stalthos-toolkit/docs/toolkit-architecture.md` — command contract, status
  envelope, exit codes 0–7, determinism rules, per-area file ownership.
- Planning docs, as needed: `Rauken-Archipelago/docs/` — `README.md`,
  `vision-and-principles.md`, `flora.md`, `creatures.md`, `geography.md`,
  `world-generation.md`, `ecology-and-hydrology.md`, `spatial-activation.md`,
  `asset-toolkit.md`.

## Non-negotiables

1. **Honesty over completeness.** An unimplemented command exits 5. A missing
   tool exits 4 with an actionable hint. A degraded path warns and marks
   `Metric.measured=False`. Never report success you did not achieve; never
   fabricate a metric, a citation, or a taxon.
2. **Determinism.** All randomness via `ctx.seed_for(...)` / `ctx.rng(...)`
   (sha256-derived). Never global `random`, never `hash()`, never an unseeded
   Blender random. Same (definition, seed, tool versions) ⇒ same bytes out.
3. **One JSON status object on stdout**, always — success and failure. Logs go
   to stderr. Every produced file is a hashed artifact with a role.
4. **`core/` is frozen** unless you are worker zero. If you need a change
   there, implement locally and report the proposal. Do not edit files another
   worker owns.
5. **Free/open source only.** No paid recurring APIs, no proprietary hard
   dependencies. Optional tools degrade to typed errors.
6. **Display fidelity beats internal fidelity.** The simplest plausible
   approximation that looks right wins. Do not turn a visual requirement into a
   demand for real hydrology, physiology or nutrient budgets.

## Conventions

- Register commands with `@command(name, summary, stage="...")`; do not hand-edit
  `cli.py`'s command list — worker zero wires new commands in at integration.
- Definitions are YAML validated against `stalthos-toolkit/contracts/*.schema.json`
  via the existing `schemas/registry.py`. **Do not write a second validator.**
- Claims are marked *Proposed* / *To test* / *Unknown* / *Decided*.
- Tests must pass **without Blender and without network**. Assert on the status
  envelope and exit codes, never on log text.
- Species are **fictional forms**. Never present them as real taxa; never invent
  DOIs, specimen numbers or museum accessions. Mark invented anatomy
  `uncertainty: speculative`.

## Verify before you claim

Run it. `PYTHONPATH=src python -m pytest -q` from `stalthos-toolkit/`, then
run the actual command and paste the real output. "Should work" is not evidence,
and your report will be re-verified in the merged tree anyway.
