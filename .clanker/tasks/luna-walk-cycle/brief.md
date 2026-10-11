# Bounded Smalltrotter walk cycle

task_id: luna-walk-cycle
objective: Generate a looping Smalltrotter walk action from the existing rig and YAML gait contact definition, preserve the neutral GLB, emit a separate animated GLB with real joint animation channels, and report machine-readable loop/contact validation.
acceptance:
- Pure gait helper produces deterministic cyclic phase/contact plans and passes offline unit tests.
- rig_creature consumes animation_contacts.walk_cycle and emits neutral plus animated GLBs and machine-readable motion metadata.
- Actual Blender run emits a structurally valid animated GLB with at least one animation and channels; loop and geometric contact evidence are recorded.
- No claim of visual quality, planted feet or Wicked compatibility exceeds the evidence.
artifacts: stalthos-toolkit/src/stalthos_toolkit/blender/scripts/rig.py; stalthos-toolkit/src/stalthos_toolkit/blender/scripts/gait.py; stalthos-toolkit/src/stalthos_toolkit/commands/rig_creature.py; stalthos-toolkit/tests/test_gait.py; .clanker/tasks/luna-walk-cycle/brief.md; .clanker/tasks/luna-walk-cycle/task-state.json; .clanker/tasks/luna-walk-cycle/receipt.json; .clanker/tasks/luna-walk-cycle/evidence.json
constraints: No core/ or cli.py edits. No planning clone edits. Use per-run derived seed, no new dependencies, pure helper tests offline. Outputs only to worktree ignored var. Preserve actual failures and distinguish proxy contact checks from visually planted feet.
authority: This worktree only. No commits, pushes, global installs or worker dispatch.
returns: C:/Users/Chris/.codex/skills/clanker-mode/contracts/clinker-receipt.schema.json
budget: Standard capability; complete by 17:32 local.
