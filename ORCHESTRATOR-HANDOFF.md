# ORCHESTRATOR HANDOFF — The Stalthosian Archipelago

**Owner:** worker zero (operator). **Audience:** the next operator session.
**Rewritten:** 2026-10-10, at `dfa7e6d`. Supersedes the version at `371b91c`.

This is the *orchestrator* document. It is not the engineering contract —
that is `stalthos-toolkit/docs/toolkit-architecture.md` (frozen). It is not
project rules — that is `AGENTS.md`. It answers one question: **where does the
project actually stand, and what is the next operator supposed to do?**

Anything marked *(verified)* was checked against the working tree in this
session. Anything not so marked is inference and is labelled.

---

## 1. The one-paragraph version

The whole toolkit is built, merged and green: **12 of 12 commands implemented,
1810 tests passing, no Blender and no network required.** This is a large change
from the previous handoff, which described a foundation with two working
commands and 172 tests. Three bodies of work are still stranded on unmerged
branches — **water optics (940 lines, untested)**, **environment heightfield
(442 lines)**, and one dead branch whose content is already on master. The
overriding operational lesson of the previous session was that **worker
processes die mid-task and take uncommitted work with them**; four separate
salvages are recorded below. Merge and verify what exists before dispatching
anything new.

---

## 2. Project shape

| Path | What it is |
|---|---|
| `stalthos-toolkit/` | **The code.** Python package, console script `stalthos`. |
| `Rauken-Archipelago/` | **Planning docs only**, separate upstream clone, git-ignored. Never commit code there. |
| `graphify-out/` | Derived knowledge graph, git-ignored. Read in place. See §7. |
| `.statefulclanker/` | Task records, intent records, worker registry, stale worktrees. Machine state, not a doc. |
| `AGENTS.md` | Shared rules every agent must obey. |
| `ORCHESTRATOR-HANDOFF.md` | This file. |

---

## 3. Verified current state *(checked this session)*

### 3.1 `master` — `dfa7e6d`, working tree clean

| Commit | What it brought |
|---|---|
| `211adb7` | foundation — `core/`, `cli.py`, architecture doc |
| `254be65` | Integration 1 — schemas, 5 contracts, `validate/`, runbook, tests |
| `cbeca02` | Integration 2 — verified reference sourcing, `reference-policy.md` |
| `648dcb8` | Integration 3 — **merged the roster: 235 plant + 50 fauna profiles** |
| `f460227` + `7c6e9e0` | Integration 4 — optimize / publish / benchmark stage (W14) |
| `a13a19c` | Headless Blender execution layer (W15, landed by worker zero) |
| `1a3d98b` | Integration review 3 acted on: 2 fixed, 2 rejected |
| `4253f05` | `require_kind` guard — refuse a definition of the wrong kind |
| `b829b4c` | Integration 5 — CLI wiring; two doc contradictions reconciled |
| `dfa7e6d` | `core/` — stop reporting a clean Blender exit as `(exit 0)` |

```
PYTHONPATH=src python -m pytest -q     # from stalthos-toolkit/
1810 passed, 2 skipped in 303.05s
```

`stalthos list` reports **12 `ok`, 0 stubs**. Every command in the frozen §2
contract is now implemented. Roster: **241 plant + 54 creature profiles**.

### 3.2 Stranded work — the main reason this doc exists

Four workers died mid-task in the previous session. Worker zero recovered what
survived. Current unmerged branches *(verified via `git branch --no-merged`)*:

| Branch | Tip | Contents | Action needed |
|---|---|---|---|
| `clanker/w-04/toolkit-headless-blender` | `b30ec5d` | ~4,500 lines, Blender layer | **Nothing.** `git diff master <branch>` is 74 insertions / 370 deletions — master strictly supersedes it. Safe to `git branch -D`. |
| `clanker/w-11/water-optics-surface-typ` | `2acf0d3` | `water/optics.py`, 940 lines, **no tests** | **Land or discard.** Highest-value unmerged asset. |
| `clanker/w-08/toolkit-terrain-hydrolog` | `e6fed6f` | `environment/heightfield.py`, 442 lines | **Dispatch to finish**, or salvage as-is. |

Legacy worktrees under `.statefulclanker/worktrees/` are stale and no longer
used by the current extension. Safe to remove.

### 3.3 The two integrations that needed judgement

Both are worth reading before you change the tests again — the second one is
easy to undo by accident.

**Integration 3 — reference coverage.** The roster went 10 → 295 profiles, so
the original "every species has a sidecar" assertion became unsatisfiable. It
was **not deleted**. It became (a) an invariant that any species declaring a
non-placeholder reference must have a sidecar, and (b) a measured floor,
`MIN_SOURCED_FRACTION` in `tests/test_references.py`, that fails if coverage
regresses. Truthful coverage is **10/295 = 3.4%**; the floor was verified to
bite. **Raise the floor as sourcing lands; never lower it to go green.**

**Integration 5 — two live doc contradictions.** `reference-policy.md` §8
required established usage rights and *no* `unknown` usage_rights before
publishing — directly contradicting intent i-0002, and an agent following it
literally would block assets the operator explicitly permits. §4 said
`internal_reference` must be excluded from published manifests while
`publish_asset` copies references verbatim into the receipt. Both reconciled:
§8 now separates blocking structural conditions from warning-only licensing
conditions; §4 distinguishes the **redistributable credits manifest** (excludes
`internal_reference`) from the **internal build receipt** (records all
provenance, per i-0002).

---

## 4. Active intent (human, authoritative — do not relitigate)

From `.statefulclanker/intent/current.json`:

- **i-0001** — Not Unreal. Blender-first, free/open-source, reproducible.
- **i-0002** — `publish_asset` must **not** hard-block on `usage_rights:
  unknown/unverified`. Warn, record provenance, allow publication. Only
  structural failures block: validation not passing, `status.partial`, missing
  artifact or hash mismatch, incomplete provenance.
- **i-0003** — Split oversized or failing workstreams. Assignment packets must
  **point at documents, files and sections already on disk** rather than
  restating them.
- **i-0004** — Match the deliverable's **failure mode**, not the task category.
  Strict-schema artifacts go to a model rated strong on structured output.
  Pin the model explicitly when the output is machine-validated.

---

## 5. Open items, in priority order

### P1 — Decide water optics (`2acf0d3`). 940 lines, untested.
The single largest unmerged asset. It uses a three-tier number discipline
(VERIFIED / REFERENCED / ARTISTIC) that is the most honest thing in the salvage
— **preserve that**. It needs a test before it lands: Beer-Lambert extinction
monotonic in depth, Fresnel rising toward grazing angles. It is pure data and
needs no Blender. Per i-0003 the natural split is optics+surfaces first, then
shader graphs as reviewable node-graph *data* plus the command and schema.

### P2 — Environment stage (`e6fed6f`) is one module of four.
`heightfield.py` exists; hydrology, biomes, the orchestrator command and the
terrain schema do not. Dispatch against the salvage rather than restarting.

### P3 — Real-tool paths are untested. The largest honesty gap.
No `gltfpack`, `meshoptimizer`, AMD Compressonator, glTF-Validator,
PresentMon, RenderDoc or ImageMagick/FFmpeg is installed. The optimize stage's
external-tool code paths are covered by **stub-executable tests only** — argv
construction, not behaviour. `benchmark_asset` reports
`frame_time_measured: false` permanently because the toolkit ships no runtime
harness, so **the 30 FPS floor is unverified for every asset in the project.**
One pass on a machine with the tools is the cheapest high-value work available.

Blender *is* installed and genuinely exercised: `tests/test_blender.py` takes
~190s because it makes real Blender 5.2 subprocess calls.

### P4 — Reference sourcing, 3.4% covered.
285 profiles carry `usage_rights: unknown` placeholders. Raise
`MIN_SOURCED_FRACTION` as waves land. Slow, honest work — never shortcut by
inventing citations.

### P5 — `render_growth_stages` cannot produce its §8 review sheet.
ImageMagick/FFmpeg absent, so it emits per-stage PNGs and no montage. A correct
degraded path, but the deliverable is genuinely missing.

### P6 — Needs an operator ruling, not a code change.
`validate/checks.py` treats an **empty** `provenance.licenses` array as FAIL,
which blocks publication indirectly via `validation != pass`. i-0002 permits
unverified rights on a *recorded* reference; an absent record is arguably a
structural provenance gap. Which side of that line it falls is yours to call.

### P7 — Unexplained, do not write off.
A `rig_creature` exit-6 transient occurred once and did not reproduce; no stack
was captured because the next successful run overwrote the log. If it recurs,
look at the `--timeout` path and log retention first.

---

## 6. Standing operator rules

1. **Never merge unreported work.** A commit on a branch is not a result.
2. **Never merge unrun.** Full suite plus one real CLI invocation, pasted.
   "Should work" is not evidence.
3. **Verify worker claims before acting.** Every report in the previous session
   contained at least one claim that turned out to be stale or already done.
   W14's "import the three command modules" and W20's unresolved list were both
   out of date by the time they arrived.
4. **`cli.py` and `core/` are yours alone.** Workers report core/ proposals; you
   decide. Two have landed this way and both were right.
5. **Dispatch pointers, not prose** (i-0003). Every worker reads from the same
   checkout; name the file and the section number.
6. **Pin the model when output is machine-validated** (i-0004).
7. **Cheapest competent mind, but check the model.** Six of seven graphify
   dispatches died or produced invalid output; the one that succeeded was the
   one pinned to a strong structured-output model.
8. **Commit early.** Every salvage in §3.2 exists only because worker zero went
   and recovered it.
9. **Check for an unclosed task record when a completion receipt repeats.**
   W17 and W20 both fired duplicate notifications because the task was left
   `active` after the worker reported success. That is a recurring extension
   behaviour, not a stalled worker.
10. **Honesty over completeness, including toward yourself.** Anything in this
    document not marked *(verified)* is inference.

---

## 7. The knowledge graph

`graphify-out/` holds a graph over both the planning docs and the code:
**3,377 nodes, 5,608 edges, 233 communities**, with `graph.json`, `graph.html`,
`GRAPH_REPORT.md` and a `manifest.json` for incremental `--update`.

Query it before reading widely:
```
graphify query "what constrains the player's authority over the animal"
```
It answers cross-document questions — e.g. hero-view → hero-sim →
simulation-architecture — and its bridge edges connect planning concepts to the
code that implements them, which is how an agent learns *why* a constraint
exists.

**Known limitations, stated rather than hidden:** the health check reports 275
dangling-endpoint and 368 collapsed edges, both characteristic of AST extraction
(imports to external modules; schemas emitting `contains`+`extends` between the
same pairs). Community labels are **derived from each node's `source_file`, not
hand-curated** — recorded in `.graphify_labels.json` for override. The 305
species YAML profiles are excluded from the semantic pass; they are structured
records already schema-validated, not prose. Token cost is recorded as 0,
because the worker dispatches died before returning counts.

---

## 8. Quick reproduction of §3

```bash
cd "E:/Stalthosian Archipelago"
git log --oneline -12
git status --short                       # expect clean
git branch --no-merged master            # expect 3 stale branches

cd stalthos-toolkit
PYTHONPATH=src python -m pytest -q
PYTHONPATH=src python -m stalthos_toolkit.cli list

cd ..
git diff --stat master clanker/w-04/toolkit-headless-blender   # master supersedes it
cat .statefulclanker/intent/current.json
python -c "import json;r=json.load(open('.statefulclanker/workers/registry.json'));[print(k,v['status']) for k,v in sorted(r['workers'].items())]"
```

## 9. Open questions for the human

- Is ~295 breadth-scale profiles the target, or is the next wave depth per
  species rather than more width?
- Is there a target asset or island count to schedule remaining stages against?
- Water: split optics+surfaces from shader graphs (per i-0003), or land the 940
  lines as-is with a test bolted on?
- Should the project acquire the external tools (gltfpack, Compressonator,
  PresentMon) so P3 can be closed properly, or continue degrading honestly?