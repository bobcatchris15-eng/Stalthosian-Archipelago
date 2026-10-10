# Blender execution layer — determinism contract

This file documents how `src/stalthos_toolkit/blender/` keeps a run
reproducible. It is the "how" behind the claims in
`docs/toolkit-architecture.md` §3.

## 1. Seeds always come from Python

`ctx.seed_for(...)` / `ctx.rng(...)` (frozen `core.seeding`) derive every child
seed from the root seed. The bpy side never invents a seed.

The algorithm itself lives in **one** pure-stdlib module,
`src/stalthos_toolkit/blender/seeds.py`, which is a byte-for-byte copy of
`core.seeding.derive_seed` (root seed normalized to decimal, labels joined with
`\x1f`, sha256, first 8 bytes big-endian, masked to 63 bits). The bpy scripts
import that same file, so the two sides of the process boundary cannot drift;
`tests/test_blender.py::test_seed_derivation_agrees_with_frozen_core` asserts it.

Inside Blender:

* the global `random` is **never** used — every generator is
  `random.Random(derive_seed(seed, *labels))`;
* `numpy.random` is never used;
* `hash()` is never used (it is salted per process);
* time (`time.time`, `bpy.app.handlers`, frame counters) is never an input to
  geometry.

`PYTHONHASHSEED=0` is set on the child process as a belt-and-braces measure.

## 2. Procedural textures have no time input

`_materials.build_procedural_material` drives all structure from
`Texture Coordinate → Object`, so the pattern is tied to the object, not to the
frame. The 4D noise nodes get an explicit `W` input equal to a constant derived
from the seed (`seed % 997`, `+31`, `+71`), never `#frame` and never a frame
driver. The scene is pinned to frame 1 (`frame_start = frame_end = 1`) so even a
mis-authored `#frame` reference is constant.

## 3. Renders have no stochastic or time-based variance

`_stalthos.configure_scene` pins, every run:

| Knob | Value | Why |
|---|---|---|
| `cycles.seed` | the seed Python derived | same sampling noise every run |
| `cycles.use_animated_seed` | `False` | no per-frame seed churn |
| `cycles.use_denoising` / `denoiser` | `True` / `OPENIMAGEDENOISE`, `denoising_use_gpu=False` | deterministic, no temporal component |
| `cycles.adaptive_threshold`, bounces, clamps | fixed literals | no adaptive schedule drift |
| `eevee.taa_render_samples` | fixed literal | no sample-count jitter |
| `render.use_motion_blur`, `eevee.use_motion_blur` | `False` | no temporal smear |
| world | an explicit background **colour + strength** | no HDRI rotation, no sky time-of-day |
| sun | fixed `azimuth`/`elevation`/`angle`, no animation | no animated sun |
| colour management | `view_transform = Standard`, `exposure = 0`, `gamma = 1`, `look = None` | no per-run look |
| frame | pinned to 1 | constant `#frame` inputs |

`render.setup_camera` places the camera from the asset's world-space bounding
box at fixed azimuths with a fixed elevation and a constant fit distance
(`extent * 1.45`) — no auto-framing that could depend on render order.

## 4. What "reproducible" means at the byte level — and what it does not

`.blend` files embed raw in-memory datablock **pointers**, so two runs of an
identical scene still differ by ~100–200 bytes of addresses even though the
scene is identical. Verified here:

```
run 1 vs run 2, same seed, same definition (ashenleaf.al1, seed 42, Blender 5.2.0 LTS):
  geometry digest: identical (883d7c3ca182ce4c63ef9bce742483a40c67ac1f742d73d4e87787b069f9470d)
  triangle count  : identical (5460)
  .blend          : 961 657 bytes both runs, 129 bytes differ (0.01%),
                    every difference on an 8-byte boundary and every one is a
                    heap address (e.g. 0x01496cf5 vs 0x018fd67e)
```

So the reproducibility evidence carried in the result JSON is a
**`geometry_digest`**: a sha256 over each object's name, every vertex position
as raw little-endian float32, every polygon's vertex indices, and the material
names. It is stable across runs, machines and Blender restarts, and it changes
when the seed changes. Every object row in `metrics.objects` carries its own
digest plus the scene-level one.

Artifact `sha256` values in the status envelope still hash the real `.blend`
bytes; the digest exists precisely because the file hash alone is not a
reproducibility proof. **Do not claim byte-identical `.blend` files**; the
geometry is reproducible, the container is not, and that is a Blender property
rather than a pipeline defect.

## 5. The source tree is never written to

* Each run gets a fresh `tempfile.mkdtemp(prefix="stalthos_blender_")` as the
  child process **CWD**.
* `BLENDER_USER_RESOURCES`, `BLENDER_USER_CONFIG`, `BLENDER_SYSTEM_DATAFILES`,
  `TEMP`, `TMP`, `TMPDIR` and `STALTHOS_TMP` all point inside that temp dir;
  Blender's `preferences.filepaths.temporary_directory` is set there too, and
  `use_auto_save_temporary_files` is off.
* `params.json` and `result.json` live in that temp dir; `result.json` is
  written atomically (`.partial` then `os.replace`).
* Only paths Python pre-created under the workspace root are written by Blender.
  `ctx.workspace.path_for()` refuses anything that escapes the root.
* `--factory-startup` plus `--disable-autoexec` stops any auto-run script in a
  loaded `.blend` and user preferences from leaking in.
* stdout/stderr are captured and written to `<workspace>/reports/logs/`.
* The temp workdir is removed after a successful run. A **failing** run keeps
  its workdir so a crash can be inspected; `--keep-workdir` (option key
  `keep_workdir`) keeps it for a successful run too. Without this the per-run
  directories accumulate indefinitely.

`tests/test_blender.py::test_result_json_is_never_written_into_the_source_tree`
asserts both the child's CWD and its result path are outside the source tree.

## 6. The JSON handoff

Python writes `params.json`: command, asset id, definition, derived seed,
stage/variant, engine, resolution, and absolute output paths.
bpy writes `result.json`: `ok`, action, seed, `blender_version`,
`blender_version_tuple`, engine, timings per stage, metrics (objects, triangle
counts, per-object and scene `geometry_digest`, family-specific counts),
artifacts, warnings, and on failure an `error` object **with a traceback**.

The result is never printed. bpy exits 0 even on a structured failure so
Python can read the failure and turn it into the right exit code
(`ExternalToolFailed`, 6) with full detail; an unhandled crash is still
non-zero and becomes exit 6 as well. A missing result JSON is an error, never a
silent success.

## 7. Definition shape: what is actually read

`blender/params.py` reads the **current** schemas
(`contracts/species-definition.schema.json`, `contracts/creature-definition.json`):

* `generator` maps to one of the five implemented families through
  `params.GENERATOR_FAMILY`. Three roster generators (`fibrous_monocot`,
  `bracket_fungus`, `moss_lichen_crust`) have **no exact generator** and are
  approximated; `params.family_is_approximation()` reports this and the
  calling command warns. The mapping is never silently exact.
* `growth_stages` is read as an ordered **array**; the last entry is the mature
  reference form and stage `height_scale` is that entry's height over the
  mature height. `proportions` drives `trunk_scale`/`branch_density`.
* `variation.ranges` maps `seed_label -> [min, max]` **multiplicative** factors
  onto `height`/`leaf_scale`/`branch_angle`. A definition with no `variation`
  block falls back to documented narrow defaults (±12–20%), never to an
  unbounded random draw.
* Creature `body_plan` is read as **shares** (`neck_length_share`,
  `tail_length_share`, `axial.ribcage_depth_share`, per-segment `length_share`)
  resolved against `life_stages[].total_length`.
* Only a **quadrupedal** posture has an implemented proxy. `build_creature`
  exits **5** for `bipedal`/`semi_bipedal` rather than emitting a confident
  quadruped for a biped.

The legacy absolute-key forms are still accepted as a fallback so hand-written
definitions keep working, but the schema shapes are the ones that are tested.

## 8. Exit-code mapping

| Situation | Result |
|---|---|
| `--blender` path missing, or Blender not discoverable | `MissingDependency` → **4** |
| bad definition / missing `-d` | `UsageError` → **2** (checked *before* the tool check) |
| `--dry-run` | plan only, exit 0, no Blender required, no artifacts |
| child exceeded `ctx.options["timeout"]` | `Interrupted` → **7** |
| child non-zero exit, or `ok: false` in the result | `ExternalToolFailed` → **6** |
| exit 0 but no `result.json` | `BlenderError` → **6** |
