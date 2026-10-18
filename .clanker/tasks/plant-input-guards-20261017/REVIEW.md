# Plant input guard review — 2026-10-17

Candidate commit: 5a0142b0b4b16073bce20113ec1005a70bafbe43
Branch: codex/plant-input-guards-20261017
Worktree: var/worktrees/plant-input-guards-20261017 (clean)
Base: 5a17394

Owned files:
- stalthos-toolkit/src/stalthos_toolkit/commands/generate_plant.py
- stalthos-toolkit/tests/test_plant_input_guards.py

Raw stage lookup now mirrors growth_stage_list: recursive params.pick for canonical
key, then life_stages or stages fallback. This preserves absolute legacy mapping
fields and legacy leaf_scale, and avoids list alias None iteration errors.
Authored scale_spray/needled foliage requires finite positive length and width
converted through existing params.unit_m. Invalid values raise typed UsageError
before external tooling; actual CLI regression checks confirm exit2 JSON and no
traceback for missing, zero, negative, NaN, positive/negative infinity.
Existing small-length routing heuristic intentionally left for root's separate
change; geometry, Blender scripts, non-tree and no-stage routes unchanged.

Focused command (PYTHONPATH=src from worktree stalthos-toolkit):
python -m pytest -q tests/test_plant_input_guards.py tests/test_generate_plant_stage_sizing.py tests/test_tree_stage_crown.py
Actual output: 54 passed in 3.62s (exit0).
Coverage includes canonical/alias stage lists and mappings, legacy absolute leaf
and trunk dimensions, legacy leaf_scale, cm/mm dimensions, no-stage behavior,
and existing Splitcrown/crown stage regressions.
Git diff --check passed. Root coordinator owns final combined full toolkit suite.
No root source/state changed. This receipt is the only allowed root task output.
