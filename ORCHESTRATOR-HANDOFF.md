# ORCHESTRATOR HANDOFF — The Stalthosian Archipelago

**Owner:** worker zero (operator). **Audience:** the next operator session.
**Written:** 2026-10-10, at integration 2 (`cbeca02`).

This is the *orchestrator* document. It is not the engineering contract —
that is `stalthos-toolkit/docs/toolkit-architecture.md` (frozen). It is not
project rules — that is `AGENTS.md`. It answers one question: **where does the
project actually stand, and what is the next operator supposed to do?**

Everything below marked *(verified)* was checked against the working tree in
this session. Anything not so marked is inference and is labelled as such.

---

## 1. The one-paragraph version

A Blender-first, free/open-source asset pipeline is being built for a fictional
Late Cretaceous archipelago. The **foundation and contract are merged and
green** (172 passed, 1 skipped, no Blender, no network). Three workstreams have
produced deliverables that are **committed in worker worktrees but not yet
merged into `master`** — the roster (~295 species profiles), the optimize /
publish / benchmark stage, and one environment module. Two workstreams
(Blender execution, water) are still running with little or nothing committed.
The independent adversarial review **was dispatched and never delivered**. The
single highest-value next action is merging the finished work, not dispatching
more.

---

## 2. Project shape

| Path | What it is |
|---|---|
| `stalthos-toolkit/` | **The code.** Python package, console script `stalthos`. |
| `Rauken-Archipelago/` | **Planning docs only**, separate upstream clone, git-ignored. Never commit code there. |
| `.statefulclanker/` | Task records, intent records, worker registry, worker git worktrees. Machine state, not a doc. |
| `AGENTS.md` | Shared rules every agent must obey. |
| `ORCHESTRATOR-HANDOFF.md` | This file. |

Pipeline stages are 1–9; the stage list and per-stage file ownership are in
`docs/toolkit-architecture.md` §2 and §5. Read that before dispatching anyone.

---

## 3. Verified current state *(checked this session)*

### 3.1 `master` — commit `cbeca02`, clean working tree

Merged so far:
- `211adb7` foundation — `core/` (contract, errors, manifest, seeding, status,
  workspace), `cli.py`, architecture doc.
- `254be65` Integration 1 — schemas/registry/loader/validator, 5 JSON Schemas,
  9 species definitions, `validate/`, `research_species`, `validate_asset`,
  pipeline runbook, `scripts/stalthos.ps1`, README, core + CLI tests.
- `cbeca02` Integration 2 — verified reference sourcing (`research/`,
  `species/**/references/*.yaml`, `docs/reference-policy.md`,
  `tests/test_references.py`); 11 unverified licence claims corrected to
  `license=unverified`, `redistributable=false`.

Test run, pasted from `stalthos-toolkit/`:
```
PYTHONPATH=src python -m pytest -q
172 passed, 1 skipped in 7.60s
```

Registered commands — `python -m stalthos_toolkit.cli list`:
- **implemented:** `research_species`, `validate_asset`
- **stubbed (exit 5, not implemented):** `generate_plant`,
  `generate_plant_variants`, `build_creature`, `rig_creature`,
  `bake_materials`, `generate_lods`, `render_turntable`,
  `render_growth_stages`, `benchmark_asset`, `publish_asset`

Blender 5.2 is installed at `C:\Program Files\Blender Foundation\Blender 5.2\`.
`gltfpack` / Compressonator presence: *not verified this session*.

### 3.2 Work in worker worktrees, **not yet in `master`** *(verified)*

`git log master..HEAD` in each worktree:

| Worker | Task | Branch commits | Status |
|---|---|---|---|
| **W09 Sporemonger** | t-0009 roster | 3 commits, 235 plant + 50 fauna profiles + `tests/test_roster.py` | **COMPLETE, unmerged** |
| **W13 lodsmith** | t-0013 optimize/publish/benchmark | 2 commits (`optimize/`, 3 commands, benchmark harness, tests) | RUNNING, substantial work landed |
| **W08 Terraformer** | t-0008 terrain/hydrology | 1 commit (`environment/heightfield.py`) | RUNNING, early |
| **W04 BlenderForge** | t-0004 headless Blender | none | RUNNING, nothing committed yet |
| **W11** | t-0011 water | none | RUNNING, nothing committed yet |
| **W10 IntegrityScout** | t-0010 independent review | none | **IDLE, never delivered — gap** |
| **W12 ReferenceArchivist** | t-0012 references | 3 commits | COMPLETE, **merged** as `cbeca02` |

Note: W09's work is *large* (295 new definition files) and it branched from
`254be65`, i.e. **before** Integration 2. Merging it will conflict with the
reference sidecar glob changes in `schemas/loader.py` and
`tests/test_schemas.py`. Expect that; it is not a reason to delay.

---

## 4. Active intent (human, authoritative — do not relitigate)

From `.statefulclanker/intent/current.json`:

- **i-0001** — This project is **not** Unreal. Blender-first, free/open-source,
  reproducible. UE skills are removed from the environment.
- **i-0002** — `publish_asset` must **not** hard-block on `usage_rights:
  unknown/unverified`. Personal, non-commercial project: warn, record
  provenance, allow publication. **Only structural failures block** —
  validation not passing, `status.partial`, missing artifact or hash mismatch,
  incomplete provenance (missing seed / input hashes / tool versions).

Anything a worker proposes that contradicts these is refused, regardless of how
reasonable it sounds.

---

## 5. The next operator's queue, in priority order

### P1 — Merge W09 (roster). Biggest single win, already finished.
Merge `clanker/w-09/roster-200-plant-50-faun`, resolve the loader/glob
conflicts against Integration 2 deliberately, then re-run the full suite and
paste the output. Only a real run counts as done. 295 hand-authored profiles
that are never merged are worth zero.

### P2 — Close the W10 review gap.
The adversarial audit of the merged foundation was assigned and **silently
died** (worker IDLE since spawn, no branch commits, no deliverable at
`docs/reviews/integration-review-1.md`). Either re-dispatch it, with a
different model, or do the honesty audit yourself in-session. Do not leave the
foundation unaudited while stacking more stages on it — that is the exact
failure mode the project's first non-negotiable is written to prevent.

### P3 — Merge W13 (optimize/publish/benchmark) once it reports.
Two prior workers on this identical scope (W05, W07) stalled and delivered
near-empty worktrees. W13 has actually landed code. When it reports, verify
against intent i-0002 specifically: unverified rights **warns**; only structural
failures **block**; degraded paths set `Metric.measured=False`.

### P4 — Nudge or restart W04 (Blender execution) and W11 (water).
Both are running with **zero commits**. W04 is on the critical path for every
stage after it — nothing generates geometry without it. If either is still
commit-free after the next check, steer it explicitly to *land one honest
increment now* (a module plus a test that passes without Blender) rather than
accumulating to the end. That failure mode has already cost this project two
workers on the same scope.

### P5 — W08 environment continues; do not merge mid-flight.
It needs `heightfield` + `hydrology` + `biomes` + the orchestrator command.
Merge when it reports, and wire `build_environment` into `cli.py` yourself
(workers never edit `cli.py`).

---

## 6. Standing operator rules

1. **Never merge unreported work.** A commit in a worktree is not a result.
   Wait for the structured result, then verify the claim yourself.
2. **Never merge unrun.** `PYTHONPATH=src python -m pytest -q` from
   `stalthos-toolkit/`, plus one real CLI invocation with the output pasted.
   "Should work" is not evidence.
3. **`cli.py` is yours alone.** Workers register with `@command(...)`; you wire
   the list at integration.
4. **`core/` is frozen.** Workers report proposed core changes; you decide.
5. **File ownership before dispatch.** Claim checkout paths at spawn so workers
   do not collide. Two optimizers already collided on the same scope.
6. **Cheapest competent mind.** Four parallel implementation workers on
   independent modules is correct; four on the *same* scope is the bug that
   produced W05 and W07.
7. **Honesty over completeness.** Exit 5 for unimplemented beats a fake pass.
   If a worker reports something done that you cannot reproduce, that is the
   finding — act on it before merging anything else from them.
8. **Claim nothing you did not run.** Same standard you hold workers to,
   including in this file: anything not marked *(verified)* here is inference.

---

## 7. Quick reproduction of the state in §3

```bash
cd "E:/Stalthosian Archipelago"
git log --oneline -5
git status --short                       # expect clean
git branch -a

cd stalthos-toolkit
PYTHONPATH=src python -m pytest -q
PYTHONPATH=src python -m stalthos_toolkit.cli list

cd ../.statefulclanker/worktrees
for w in W04 W08 W09 W11 W13; do (cd $w && echo "== $w" && git log --oneline master..HEAD); done

cat ../intent/current.json
python -c "import json;r=json.load(open('../workers/registry.json'));[print(k,v['status'],v.get('last_activity')) for k,v in r['workers'].items()]"
```

## 8. Open questions for the human

- Is the ~295-profile roster the intended breadth, or should the next wave be
  deeper per-species rather than wider?
- Do we have a target asset count / island count to schedule the remaining
  stages against, or is that still open?
- W11 (water) was called the project's biggest content gap. If it stalls like
  W04, is it worth splitting optics (pure data, low risk) from shader-graph
  generation?