# Luna creature readiness: animated creature GLB

Date: 2026-10-11
Worktree: `var/worktrees/luna-creature-readiness` (`codex/creature-readiness`)
Scope: readiness probe only; no source files changed.

## Findings

Existing candidate route is `build_creature` then `rig_creature`. The former generates a quadrupedal verification proxy from a creature YAML. The latter builds an armature, applies automatic weights and writes a neutral-pose GLB. This is usable as an export/rig baseline, but does not produce an animated creature.

`stalthos doctor` found Blender 5.2.0 at `C:\Program Files\Blender Foundation\Blender 5.2\blender.exe`, Python 3.14.5, and gltf-transform 4.5.1. `stalthos list` marks both commands implemented. The real help commands expose `--definition`, `--seed`, `--stage`, `--workspace`, `--blender`, and `--timeout`.

The selected input `stalthos-toolkit/species/creatures/smalltrotter.yaml` is a fictional quadruped with authored gaits and `animation_contacts.walk_cycle`, making it the best immediate target. `crestrunner.cr2.yaml` is bipedal and is outside the current proxy's supported postures. The root planning repository `Rauken-Archipelago/` was not modified.

## Run evidence

Both real Blender commands succeeded in this isolated worktree using `PYTHONPATH` resolved to this worktree's `stalthos-toolkit/src` and workspace `var/luna-creature-readiness`:

- `build_creature --definition stalthos-toolkit/species/creatures/smalltrotter.yaml --seed 42 --stage independent_juvenile --timeout 110`: exit 0 in 1.839 s. Wrote a 667,455-byte `.blend`; proxy body is 907 vertices / 1,730 triangles; 13 trunk, 15 neck, 32 tail segments; 4 limbs. Integument proxy is empty and warnings explicitly call this an anatomical verification proxy, not fossil measurements.
- `rig_creature` with the same input/options: exit 0 in 2.129 s. Wrote an 895,263-byte rig `.blend` and 84,476-byte neutral-pose GLB. Rig reports 74 deform bones, 907 weighted vertices, automatic weighting, neutral pose true.
- Parsed the produced GLB with `stalthos_toolkit.optimize.glb.GlbDocument` and `rig_summary`: 1 mesh, 77 nodes, 1 skin, 74 joints, 0 animations, structurally valid skin. The rig script explicitly exports with `export_animations=False`.

The YAML's gait/contact labels are planning inputs only; the current generator/rig implementation does not consume them to keyframe a gait. Current source mesh is still coarse proxy geometry, not production topology or a completed creature asset.

## Exact next implementation task

Implement the first authored quadruped locomotion clip, `walk_cycle`, for the Smalltrotter source `.blend`, driven by the existing `animation_contacts` gait definition and rig. Preserve the neutral pose, generate a looping walk action with planted foot contacts and an explicit GLB animation export, and add machine-readable clip metadata/validation that fails if the GLB has no animation channels or the loop/contact checks fail. Keep the current neutral export behavior available as a separate output.

Recommended bounded source ownership: `stalthos-toolkit/src/stalthos_toolkit/blender/scripts/rig.py` plus a new local helper under `stalthos-toolkit/src/stalthos_toolkit/blender/scripts/` for gait construction/export; if a command-level option or artifact role is needed, propose it to worker zero rather than editing frozen `cli.py`/core. Relevant tests: existing Blender rig tests in `stalthos-toolkit/tests/test_blender.py`; add only checks for action channels, export animation count, and foot-contact loop constraints. Inputs are `stalthos-toolkit/species/creatures/smalltrotter.yaml` and the just-generated rig `.blend` under ignored `var/luna-creature-readiness`.

Topology/rig gaps before a production-ready animated creature: proxy-only geometry, empty integument mesh, no detailed authored anatomy/deformation zones, no demonstrated joint deformation quality beyond successful automatic weights, no gait action or glTF animation channels, and no visual contact review. This probe does not establish Wicked Engine import/render behavior, visual quality, target-hardware performance, or final draft acceptance.

## Reproduction

From this worktree, set `PYTHONPATH` to the absolute `stalthos-toolkit/src` path, then run:

```powershell
python -m stalthos_toolkit.cli doctor --workspace var\luna-creature-readiness
python -m stalthos_toolkit.cli list
python -m stalthos_toolkit.cli build_creature --workspace var\luna-creature-readiness --definition stalthos-toolkit\species\creatures\smalltrotter.yaml --seed 42 --stage independent_juvenile --timeout 110
python -m stalthos_toolkit.cli rig_creature --workspace var\luna-creature-readiness --definition stalthos-toolkit\species\creatures\smalltrotter.yaml --seed 42 --stage independent_juvenile --timeout 110
```

All generated source, GLB, logs, results and manifests remain under this worktree's ignored `var/`.
