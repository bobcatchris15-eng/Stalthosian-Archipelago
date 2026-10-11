# Project readiness audit — 2026-10-11

**Scope:** Planning for a full first draft of the actual Stalthosian Archipelago game, including the existing asset toolkit. The draft target includes all five playable lives, four islands and connected runtime systems. Reference asset tests are preparation and validation within that scope. Read-only source inspection plus tests and isolated generated sample outputs. No source-code fixes, installs, merges, publication or engine selection performed.

## Verified baseline

- Root HEAD: `abe3b3f` at audit time. Pre-existing dirty/untracked environment, water, schema and optimizer changes retained.
- 21 planning Markdown documents inventoried; their exact input hashes are in `var/planning-audit-20261011/reports/readiness/planning-inputs.json`.
- Installed `stalthos 0.1.0`; Python 3.14.5.
- Actual suite from `stalthos-toolkit/`, with `$env:PYTHONPATH='src'`:

```text
python -m pytest -q
1908 passed, 2 skipped in 267.51s (0:04:27)
```

Process exit: 0. These results apply to the current working tree, including its existing modifications. Two tests were skipped; no claim of exhaustive external-tool coverage follows.

## Real commands and outputs

Doctor and list both exited 0. Their stdout/stderr are saved under `var/planning-audit-20261011/reports/readiness/`.

Doctor discovers Blender 5.2, FFmpeg 9.0.2 and Git. It does not discover gltfpack, meshoptimizer, Compressonator, KTX or ImageMagick. Additional optimizer-tool discovery finds glTF Transform at `C:\Users\Chris\AppData\Roaming\npm\gltf-transform.CMD`; glTF validator, PresentMon and RenderDoc are not discoverable. Absence from discovery is not proof that no installation exists anywhere on disk. LFS CLI reports 3.7.1. .NET resolves on PATH; CMake did not resolve in the scoped check. Visual Studio build-tool availability and installed engines remain unaudited.

All twelve stage commands have registered implementations. `list` emits a text table, an exception to the architecture's stated universal JSON stdout rule that should be reconciled explicitly.

Commands actually run, using an isolated workspace:

```powershell
$env:PYTHONPATH='src'
python -m stalthos_toolkit.cli --workspace var/planning-audit-20261011 research_species -d species/plants/softbough.yaml --seed 42
python -m stalthos_toolkit.cli --workspace var/planning-audit-20261011 generate_plant -d species/plants/softbough.yaml --seed 42 --timeout 90
```

Selected fields from their real JSON output (abridged here; not a replacement status envelope):

```json
{
  "command": "research_species",
  "ok": true,
  "exit_code": 0,
  "seed": 42,
  "asset_id": "plant.softbough.sh1",
  "metrics": {
    "references": 3,
    "references_verified": 2,
    "references_unverified": 1,
    "unresolved_locators": 3,
    "uncertainty": "unverified",
    "offline": true
  }
}
```

Unresolved locators: `Rauken-Archipelago/docs/flora.md`, `Rauken-Archipelago/docs/plant-visual-catalog.md`, and `species/plants/plant.softbough.sh1/reference/twig_arc.jpg`. The planning docs exist from the root; resolution from the documented toolkit cwd needs correction. The photo needs separate existence/provenance review. The command normalized an offline record and did not verify new research online.

```json
{
  "command": "generate_plant",
  "ok": true,
  "exit_code": 0,
  "seed": 42,
  "asset_id": "plant.softbough.sh1",
  "duration_s": 2.312,
  "partial": false,
  "metrics": {
    "triangles": 2592,
    "vertices": 1584,
    "blender": "5.2.0 LTS",
    "engine": "EEVEE"
  }
}
```

The generated `.blend` is at `var/planning-audit-20261011/sources/plant.softbough.sh1/plant.softbough.sh1.blend`; SHA256 `1baa6b074f6fbbf8c44e0e7676b0ad7e85e28cb31391148b776dd2a2580e0b52`. Source result JSON, manifest, research report and Blender log are also preserved in the workspace. Counts are reported source geometry counts; evaluated modifier/runtime geometry and appearance were not accepted here.

## Material readiness gaps

1. **Toolkit installation is usable, pipeline readiness is partial.** A real shrub generation ran. Full generation-to-game chains, bake/review quality and repeat-run byte determinism remain to test.
2. **Creature generation is a quadrupedal proxy.** `commands/build_creature.py` explicitly supports `quadrupedal` and `cursorial_quadruped`; other body plans exit 5. Its existence does not establish bipedal/flying/serpentine production support.
3. **295 profiles are data, not accepted content.** Current disk count: 241 plant and 54 creature YAML files. Morphology diversity, anatomy, source coverage, materials and motion require review.
4. **Optimization completeness is limited.** `generate_lods.py` explicitly documents an impostor JSON stub, decimated-shell collision and degraded paths when real tools are missing. Configure real tools and prove output quality before broad production.
5. **Runtime frame-time benchmark is absent.** `benchmark/measure.py` measures file geometry/bytes but has no running-scene harness and reports frame-time unmeasured. Installing a capture tool alone does not fix this.
6. **Terrain/water salvage is unfinished integration.** Heightfield, hydrology, biomes and water optics import on this machine. Terrain schema and modules include existing uncommitted work; there is no corresponding registered environment/water command in the twelve-stage CLI. Import success does not validate calculations, authored geography or rendering.
7. **Dependency declaration requires review.** Existing environment modules import Pydantic while `pyproject.toml` declares PyYAML as its sole runtime dependency. A clean environment may behave differently from this machine.
8. **Operational docs are stale.** README still says all stage commands are unimplemented; runbook/handoff availability and status statements also need reconciliation with current checks.
9. **Final image direction and engine remain undecided.** Visual references are open in design. Flax preference must be reconciled with free/open-source hard-dependency requirements; see the root plan's official-source links.

## Next work

Follow the full draft delivery contract in `PROJECT-BUILD-PLAN.md` at the root: close invocation/provenance and real-tool gaps, prove the six plants and Smalltrotter sample chains, finish existing terrain/water work, create the same-scene import/capture harness, then choose a runtime using measured evidence. Continue through complete system/lifecycle coverage, all five heroes and four-island integration to deliver the full draft; reference samples are intermediate checks. Preserve existing dirty work and retain failed receipts.


## Construction follow-through — 2026-10-11

Wicked was selected directly by the user. `stalthos-runtime/` now contains a pinned native C++17/DX12 host and six domain scenario groups. Planning inputs remain untouched.

Reference lookup now recognizes definition-local files, explicit project/docs roots and the enclosing toolkit/project. A real root research command resolves all three planning-document references; the authored external photo locator remains unresolved and is reported honestly. Pydantic 2 is declared as a runtime dependency. The integrated suite after these changes passed **1916 tests, 2 skipped**, in 273.25 seconds.

A real `.blend` to GLB to Wicked attempt exposed the missing Blender export bridge. The runner now provides `export_gltf`, preserves existing output on export failure, validates the GLB header and registers hashed export/log artifacts; the LOD command propagates its Context and timeout. Real Blender 5.2 exported Softbough to a 99,300-byte GLB, which Wicked imported as two objects.

Remaining production gap: default four-level Softbough LOD generation exhausts the fallback triangle budget at LOD2 and exits 2. `--levels 2 --allow-degraded` completes, with 2592/23 triangles and an explicit unverified simplification warning. This is a diagnostic import fixture, not accepted production LOD quality. gltfpack/meshoptimizer and Compressonator remain unavailable to this invocation. After export integration, the final integrated suite passed **1922 tests, 2 skipped**, in 277.88 seconds.

Evidence: root `var/reports/toolkit-integrated-tests-20261011.log`, `var/reports/softbough-lods-20261011.json`, and bounded run directories under `var/reports/runtime/`. Independent review rejected then revalidated incomplete-cache and hero-identity boundary cases; both corrections now pass. Frame-time, animal appearance and full-draft gates remain open.
