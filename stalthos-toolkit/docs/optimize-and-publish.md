# Optimize, Benchmark & Publish — stage 6 / 8 / 9

**Status:** *Decided* (implemented, verified without Blender, gltfpack or Compressonator)
**Owning stage:** Optimize / publish (stage worker C modules: `optimize/`, `benchmark/`,
`commands/generate_lods.py`, `benchmark_asset.py`, `publish_asset.py`)

This is the runbook for the tail of the pipeline. `docs/toolkit-pipeline.md` is the
stage→command index; this file records what the three tail commands actually do, what
they refuse to claim, and which parts are deliberately *not* implemented.

---

## 0. The one rule

> **Measure, never infer. Warn, never silently ship.**

Every number in the envelope carries a `measured` flag. Anything the toolkit cannot
observe on this machine is reported with `measured=False` and a reason. A missing
optional tool is **data, not an exception**: it produces a typed warning and a
`not-available` tool version, or — where the plan demands it — exit 4.

The second half of the rule is the licensing rule, and it is set by human intent
**i-0002** (`.statefulclanker/intent/current.json`):

> `publish_asset` must NOT hard-block on `usage_rights: unknown` or `unverified`.
> This is a personal, non-commercial project: warn, record the provenance in the
> receipt, and allow publication. Only **structural** failures block.

This overrides `docs/reference-policy.md` §8, which lists "all references have
established usage rights" as a pre-publish checkbox. Where the two disagree,
i-0002 wins and the discrepancy is recorded rather than silently resolved.

---

## 1. `generate_lods` — stage 6

Reads the source geometry recorded in an asset manifest and writes, under
`var/intermediate/<kind-dir>/<asset_id>/`:

| File | Role | What it actually is |
|---|---|---|
| `lod0.glb` … `lodN.glb` | `runtime` | the chain, strictly decreasing triangle counts |
| `collision.glb` | `collision` | a decimated shell — **not** a convex hull, and it says so |
| `impostor.json` | `intermediate` | an honest stub descriptor; **no impostor bake is implemented** |
| `textures/*` | `texture` | BC-compressed via Compressonator, or an explicitly-flagged passthrough |

### 1.1 LOD policies

| Kind | Triangle retention | Collision budget |
|---|---|---|
| `plant` | 1.00 / 0.40 / 0.15 / 0.05 | 512 tris |
| `creature` | 1.00 / 0.50 / 0.20 / 0.08 | 512 tris |
| `prop` | 1.00 / 0.45 / 0.18 / 0.07 | 512 tris |
| `scene` | 1.00 / 0.60 / 0.30 / 0.15 | 512 tris |

`--levels N` truncates or extends the chain (extra levels decay the last ratio ×0.4).

### 1.2 Simplifier truth

`LodRecord.simplifier` names the thing that **actually produced that file**:

| Value | Meaning |
|---|---|
| `gltfpack` | meshoptimizer simplified this level. Only levels ≥1 can carry it. |
| `python` | the pure-python clustering decimator ran (degraded). |
| `source_copy` | **LOD0 only** — a verbatim re-encode of the source geometry. No optimizer ever opens it. |

`lod_optimizer_applied.<level>` is recorded `measured=False` for every level that was
not produced by gltfpack, so a reader of the manifest cannot mistake a source copy for
an optimized asset.

### 1.3 Degradation

| Missing tool | Behaviour |
|---|---|
| gltfpack / meshoptimizer | chain built with the python decimator; `simplifier="python"`; every quality metric `measured=False`; loud warning naming the install hint |
| AMD Compressonator | **exit 4** by default. With degraded mode enabled: passthrough copy, `compressed=False`, `degraded=True`, warning, compression metrics `measured=False` |
| glTF validator | structural check only; `validation.external.reason` records why |
| Blender (`.blend` source only) | exit 4 with a hint — the export path belongs to the generation stage |

A degraded passthrough texture keeps its **real** container suffix. An uncompressed PNG
is never written out as `albedo.ktx2`.

### 1.4 Degraded-mode flag

`allow_degraded` is read, in order, from `ctx.options["allow_degraded"]`, then
`STEALTHOS_ALLOW_DEGRADED=1`, then the strict default `False`. The CLI does not
currently declare `--allow-degraded`; wiring it is a `cli.py` change (worker zero).

### 1.5 Exit codes

`0` ok · `2` no manifest / no source geometry · `3` chain failed its structural
self-check (non-monotonic counts, rig integrity) · `4` Compressonator or Blender
absent · `5` unimplemented · `6` an external tool ran and failed · `7` timeout.

---

## 2. `benchmark_asset` — stage 8

Writes `var/reports/benchmark/<asset_id>.json`. **Measured offline from the files on
disk**, never inferred.

### 2.1 Measured

Per-LOD triangle / vertex / material / texture / image / draw-call counts (decoded
from GLB accessors), per-LOD file bytes, live-buffer-view byte sums, chain totals,
collision proxy cost, texture bytes, and before/after deltas against the source asset.

### 2.2 Explicitly NOT measured

| Quantity | Why | Reported as |
|---|---|---|
| `frame_ms_p50/p95/p99`, `fps` | needs PresentMon/RenderDoc **and** a running scene; the toolkit ships no renderer harness | `null` + `measured: false` + a reason naming the tool |
| driver-side VRAM residency | buffer-view sum is a lower bound, not driver accounting | `measured: false`, flagged `vram_is_estimate` |
| streaming wall-clock | needs a measured disk/IO path | `null`, `measured: false` |
| silhouette drift | needs a rendered before/after | `measured: false` |
| convex-hull volume / containment error | not implemented | `measured: false` |

Frame time is **never** invented. On a machine with no capture tool the report says
so and reports nothing.

### 2.3 Budget context

Every report states the budget it was measured against, from
`Rauken-Archipelago/docs/rendering-experiments.md`:

> **NVIDIA GTX 1080 8 GB, first-generation Ryzen, 1920×1080, 30 FPS floor
> = 33.33 ms for the whole scene** — not per asset. Geometry and texture budgets are
> per asset; the frame budget covers terrain, foliage, fauna, hero beasts, weather
> and streaming together.

### 2.4 Exit codes

`0` measured (with honest unmeasured gaps) · `2` nothing to measure (no non-source
geometry artifact) · `3` an asset could not be decoded · `7` timeout. Exit 4 is never
used for a missing profiler: profilers are optional by design, and their absence is a
reported gap, not a failure.

---

## 3. `publish_asset` — stage 9

Promotes validated artifacts from `var/intermediate/` into `var/runtime/<asset_id>/`
and writes a build receipt to `var/reports/receipts/<asset_id>.json`.

Promoted roles: `intermediate`, `runtime`, `texture`, `collision`, `rig`, `animation`.
Retained in place: `source`, `preview`, `report`.

### 3.1 What blocks (exit 3, `partial: true`)

Only **structural** failures:

1. `manifest.validation.outcome != "pass"` (including `not_run`)
2. any recorded upstream status with `partial: true`
3. a promoted artifact missing on disk, unhashed, or sha256-mismatched
4. incomplete provenance: `seed` not an integer, `input_hashes` empty or not sha256,
   or `tool_versions` empty
5. `PromotionCollision` — two manifest artifacts flatten onto the same runtime path,
   which would silently overwrite one another and leave the receipt listing more
   artifacts than files exist

### 3.2 What does NOT block (i-0002)

* `references[].usage_rights` of `unknown` / `unverified` / `unestablished` / absent —
  warned about, and the offending ids are recorded in the receipt under
  `licensing.unverified_usage_rights` with `usage_rights_block_publication: false`.
* No license provenance on the manifest — warned about, recorded.
* A degraded LOD chain or an uncompressed texture — warned about per artifact, and
  recorded in the receipt's `degradation` block.

### 3.3 The receipt carries

`schema_version`, `asset_id`, `kind`, `stage`, `variant`, `published_at`,
`content_hash` (the semantic hash `validate/checks.py` also computes), `seed`,
`tool_versions` (at manifest time) + `publish_tool_versions`, `input_hashes`, the
published artifact list with role/sha256/bytes/`importer_settings`, the full
`licensing` block (licenses, references, unverified ids, policy), `degradation`
(allow_degraded flag + warnings), `validation.outcome`, and `notes`.

`content_hash` is the reproducibility anchor: `Manifest.content_hash()` ignores
timestamps and tool versions so two runs can be compared semantically.

### 3.4 Exit codes

`0` published · `2` no manifest selected / unusable manifest · `3` blocked (any
structural blocker above) · `5` unimplemented · `6` a copy failed.

---

## 4. Determinism

* No global `random`, no `hash()`, no unseeded randomness anywhere in
  `optimize/`, `benchmark/` or the three commands. Verified by inspection:
  `grep -rn "import random\|hash(\|time\.time" optimize/ benchmark/ commands/{generate_lods,publish_asset,benchmark_asset}.py`
  returns only the `semantic_content_hash` *function name*.
* The python decimator derives its grid from the data and uses first-encounter
  ordering — there is no RNG to seed.
* `datetime.now()` appears once, in the receipt's `published_at` provenance stamp,
  which is outside the content hash.
* Verified empirically: two consecutive `generate_lods` runs over the same input
  produce byte-identical `var/intermediate/` trees.

---

## 5. External tools

| Tool | Used for | If absent |
|---|---|---|
| `gltfpack` / meshoptimizer | LOD simplification | python decimator, chain flagged degraded, `measured=False` |
| AMD Compressonator | BC4/BC5/BC7 texture compression | exit 4, or flagged passthrough in degraded mode |
| `gltf-validator` / `gltf-transform` | external glTF validation | structural check only, reason recorded |
| Blender | `.blend` → `.glb` export | exit 4 with a hint |
| PresentMon / RenderDoc / Tracy | frame time | **not required**; frame time reported as not measured |

Discovery honours `STEALTHOS_<TOOLNAME>` (e.g. `STEALTHOS_GLTFPACK`) so an operator
can point at a specific binary without editing `PATH`.

---

## 6. Known gaps (*Proposed* / *To test*)

* **Not implemented:** convex hull / decomposition for the collision proxy (a
  decimated shell is produced and labelled as such); impostor baking (a parameter
  stub is written); silhouette-drift measurement; any real frame-time capture.
* **Unverified against real tools.** Every test in this stage runs with no gltfpack,
  no Compressonator and no validator, per `toolkit-architecture.md` §7. The
  argv construction is covered by stub-executable tests; the tools' own output has
  not been observed. First real-machine run should be treated as *To test*.
* **Engine deployment** is out of scope; the publish target is `var/runtime/`.
* **`docs/toolkit-pipeline.md` still says these three commands "will exit 5"** — that
  text is stale and needs worker D / worker zero to update.
* **Policy conflict:** `docs/reference-policy.md` §8 still lists established usage
  rights as a pre-publish checkbox, which i-0002 overrides for publication. Left as
  written rather than silently edited.