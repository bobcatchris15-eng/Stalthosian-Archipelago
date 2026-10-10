# ORCHESTRATOR HANDOFF — The Stalthosian Archipelago

**Owner:** worker zero (operator). **Audience:** the next operator session.
**Last rewritten:** 2026-10-10, after Integration 3 (`648dcb8`).

This is the *orchestrator* document. It is not the engineering contract —
that is `stalthos-toolkit/docs/toolkit-architecture.md` (frozen). It is not
project rules — that is `AGENTS.md`. It answers one question: **where does the
project actually stand, and what is the next operator supposed to do?**

Anything marked *(verified)* was checked against the working tree in this
session. Anything not so marked is inference and is labelled as such.

---

## 1. The one-paragraph version

The foundation, contract and roster are merged and green (1700 passed,
1 skipped). Three large bodies of work exist on **salvaged branches that no
worker finished** — the headless Blender layer (~4,500 lines), the optimize /
publish / benchmark stage, and 940 lines of water optics. They are unreviewed
and unmerged. Workers now share a single checkout, and three bounded
integration tasks are running against those branches. The next operator's job
is to **verify and land them**, not to spawn more.

---

## 2. Project shape

| Path | What it is |
|---|---|
| `stalthos-toolkit/` | **The code.** Python package, console script `stalthos`. |
| `Rauken-Archipelago/` | **Planning docs only**, separate upstream clone, git-ignored. Never commit code there. |
| `.statefulclanker/` | Task records, intent records, worker registry, legacy git worktrees. Machine state, not a doc. |
| `AGENTS.md` | Shared rules every agent must obey. |
| `ORCHESTRATOR-HANDOFF.md` | This file. |

Read `docs/toolkit-architecture.md` before dispatching anyone. §2 is the
command contract, §2.3 the exit-code table, §5 per-stage file ownership.

---

## 3. Verified current state *(checked this session)*

### 3.1 `master` — `648dcb8`, working tree clean

| Commit | What it brought |
|---|---|
| `211adb7` | foundation — `core/`, `cli.py`, architecture doc |
| `254be65` | Integration 1 — schemas, 5 contracts, 9 species, `validate/`, runbook, `stalthos.ps1`, tests |
| `cbeca02` | Integration 2 — verified reference sourcing, `docs/reference-policy.md` |
| `648dcb8` | Integration 3 — **merged the W09 roster: 235 plant + 50 fauna profiles** |
| `371b91c` | this handoff doc, first version |

```
PYTHONPATH=src python -m pytest -q     # from stalthos-toolkit/
1700 passed, 1 skipped in 101.56s
```

Registered commands (`python -m stalthos_toolkit.cli list`): only
`research_species` and `validate_asset` report `ok`. The other ten are honest
exit-5 stubs. Blender 5.2 is at
`C:\Program Files\Blender Foundation\Blender 5.2\`.

### 3.2 The two conflicts Integration 3 had to resolve

Both were caused by W09 branching at `254be65`, before reference sourcing
landed. Both are worth reading, because the second one is a judgement call a
future operator could easily undo by accident.

1. **`tests/test_roster.py::_yaml_files`** globbed `species/**/references/*.yaml`
   sidecars as if they were species definitions — 50 test failures. Fixed to
   mirror the exclusion already in `schemas/loader.py`.

2. **Reference sidecar coverage.** The roster went 10 → 295 profiles, so
   W12's "every species has a sidecar" assertion became unsatisfiable. It was
   **not deleted**. It became two things:
   - an invariant: any species declaring a non-placeholder reference must have
     a sidecar;
   - a **measured floor**, `MIN_SOURCED_FRACTION` in `tests/test_references.py`,
     which fails if coverage regresses.

   Truthful coverage is **10/295 = 3.4%**. Verified the floor bites: deleting
   one sidecar fails the test. **Raise the floor as sourcing lands; never lower
   it to go green.** Closing the gap is real queued work, not waived.

3. `test_six_reference_plants_are_present` asserted exactly 6 plants; it now
   asserts the six are *present*, which was the actual intent.

### 3.3 Salvaged branches — unmerged, unreviewed

A worker-process death (extension upgrade) killed W04, W08, W11 and W13 with
work stranded and uncommitted. Worker zero committed the stranded work to its
branch purely to preserve it. **None of it is reviewed.**

| Branch | Tip | Contents | State when stranded |
|---|---|---|---|
| `clanker/w-04/toolkit-headless-blender` | `b30ec5d` | `blender/` runner + seeds + params + pipeline, 5 bpy scripts, 7 command impls, 722-line test | ~4,500 lines; 50 tests passed in ~200 s |
| `clanker/w-13/toolkit-optimization-pub` | `7ff1dc0` | `optimize/`, `benchmark/`, 3 commands, tests | 188 passed, 1 failed |
| `clanker/w-11/water-optics-surface-typ` | `2acf0d3` | `water/optics.py` only | 940 lines, **no tests** |
| `clanker/w-08/toolkit-terrain-hydrolog` | `e6fed6f` | `environment/heightfield.py` | 1 commit, early |
| `clanker/w-09/roster-200-plant-50-faun` | — | **merged** as `648dcb8` | — |
| `clanker/w-12/research-verified-public` | — | **merged** as `cbeca02` | — |

Legacy worktrees under `.statefulclanker/worktrees/` are stale. New workers do
not use them.

---

## 4. Active intent (human, authoritative — do not relitigate)

From `.statefulclanker/intent/current.json`:

- **i-0001** — Not Unreal. Blender-first, free/open-source, reproducible.
- **i-0002** — `publish_asset` must **not** hard-block on `usage_rights:
  unknown/unverified`. Personal non-commercial project: warn, record
  provenance, allow publication. **Only structural failures block** —
  validation not passing, `status.partial`, missing artifact or hash mismatch,
  incomplete provenance (missing seed / input hashes / tool versions).
- **i-0003** — Split oversized or failing workstreams. Assignment packets must
  **point at documents, files and sections already on disk** rather than
  restating them; workers share one checkout and can read the repo themselves.

---

## 5. Running work *(dispatched this session)*

| Worker | Model | Task | Scope |
|---|---|---|---|
| **W14 Geargrinder** | space-bunny | t-0014 | Verify + land `clanker/w-13` (optimize/publish/benchmark) onto master. Must explicitly prove intent i-0002 holds. |
| **W15 Forgehead** | space-bunny | t-0015 | Verify + land `clanker/w-04` (headless Blender layer) onto master, with a real Blender 5.2 run. |
| **W17** | nemotron-3-super-120b | t-0017 | Independent adversarial audit → `docs/reviews/integration-review-3.md`. Read-only. |

W17's model was pinned explicitly. The default selection put an independent
review on the same family that wrote much of the code; that defeats the point
of independence. W12 (Nemotron) produced the strongest deliverable of the
previous wave.

Watch item: W14 claimed `tests/test_validate.py`, which is outside the scope
it was given. If it starts rewriting acceptance checks rather than reading them,
steer it back.

---

## 6. The next operator's queue, in priority order

### P1 — Land W15 (Blender layer). Critical path.
Nothing generates geometry without it. Its branch is based on `211adb7`, so the
merge conflicts across `blender/__init__.py`, seven command modules and
`tests/`. Verify the determinism claim yourself: every seed must originate in
Python via `ctx.seed_for`/`ctx.rng` and be passed *into* Blender as a params
JSON. A `bpy`-side default random breaks the project's first determinism rule
and is invisible in a passing suite.

### P2 — Land W14 (optimize/publish/benchmark). Nearest to done.
Its one known failure is `test_benchmark_asset_exit_2_without_geometry`, which
asserts a substring of a human-readable message. AGENTS.md says tests assert on
the envelope and exit codes, never log text — if that assertion is reading log
text, it should be rewritten to read the envelope rather than the message being
bent to match the test. Then check the i-0002 rule explicitly in the code.

### P3 — Act on W17's review before stacking more on top.
It is read-only and independent by design. Its findings are evidence, not
authority — verify before you fix.

### P4 — Water, in two slices. Not yet dispatched.
`water/optics.py` (940 lines, salvage `2acf0d3`) is **untested**. Split:
- **W-a — optics + surfaces:** finish `optics.py`, add `water/surfaces.py` using
  the *real* `affordances`/`habitat_tags` field names from `species/**/*.yaml`,
  plus `tests/test_water.py` that passes with no Blender and no network. No
  shader graphs, no schema, no command.
- **W-b — shaders + command:** `water/shaders.py` as reviewable node-graph
  *data*, `contracts/water-definition.schema.json`, and
  `commands/build_water_surfaces.py`. Depends on W-a; dispatch after it lands.

The optics file already carries a three-tier number discipline (VERIFIED /
REFERENCED / ARTISTIC). Preserve it — it is the most honest thing in the
salvage.

### P5 — Environment (W08 salvage). Continue from `e6fed6f`.
Heightfield exists; hydrology, biomes, the orchestrator command and the
terrain schema do not. `cli.py` wiring is always worker zero's job.

### P6 — Close the reference sourcing gap.
285 of 295 profiles have no sidecar and carry `usage_rights: unknown`
placeholders. Raise `MIN_SOURCED_FRACTION` as real waves land. This is slow,
honest work; it must never be shortcut by inventing citations.

---

## 7. Standing operator rules

1. **Never merge unreported work.** A commit on a branch is not a result. Wait
   for the structured result, then verify the claim yourself.
2. **Never merge unrun.** `PYTHONPATH=src python -m pytest -q` from
   `stalthos-toolkit/`, plus one real CLI invocation with the output pasted.
   "Should work" is not evidence.
3. **`cli.py` is yours alone.** Workers register with `@command(...)`; you wire
   the list at integration.
4. **`core/` is frozen.** Workers report proposed core changes; you decide.
5. **Claim checkout paths at spawn.** All workers share one checkout now, so
   ownership is advisory rather than enforced — it is how collisions get noticed.
6. **Cheapest competent mind, but check the model.** Three parallel workers on
   independent modules is right. Pin a model explicitly when independence
   matters; do not let default selection quietly return the family under review.
7. **Split on evidence.** W05 and W07 died on one oversized scope; W04 and W11
   died on oversized scopes with nothing committed. If a worker's branch is
   still commit-free after a dispatch, the scope is too big — split it and say
   so, rather than steering harder.
8. **Commit early.** The salvage in §3.3 exists only because worker zero went
   and recovered it. Every one of those commits carries the note "committed by
   worker zero purely to preserve in-flight work; NOT reviewed".
9. **Honesty over completeness, including toward yourself.** Anything in this
   document not marked *(verified)* is inference. Say so when you rely on it.

---

## 8. Quick reproduction of §3

```bash
cd "E:/Stalthosian Archipelago"
git log --oneline -6
git status --short                       # expect clean
git branch -a

cd stalthos-toolkit
PYTHONPATH=src python -m pytest -q
PYTHONPATH=src python -m stalthos_toolkit.cli list

cd ..
git log --oneline master..clanker/w-04/toolkit-headless-blender
git log --oneline master..clanker/w-13/toolkit-optimization-pub
git log --oneline master..clanker/w-11/water-optics-surface-typ

cat .statefulclanker/intent/current.json
```

## 9. Open questions for the human

- Is ~295 breadth-scale profiles the target, or is the next wave depth per
  species rather than more width?
- Is there a target asset or island count to schedule the remaining stages
  against, or is that still open?
- Reference sourcing is 3.4% covered. Do you want that pursued as a continuous
  background stream, or batched per lineage?
