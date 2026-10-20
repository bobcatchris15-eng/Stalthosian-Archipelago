# Ordinary exploration: connected working draft

When needs are satisfied and no direct input or player destination owns movement,
the hero now takes short local dry steps and pauses. HeroExploration uses the
existing single HeroSim goal and ordinary body integrator. Proposed tunables are
1.5m step, .25m arrival, 3 physical seconds initial wait/pause and a30-second leg
limit. Four headings are relative to the body's starting bearing; the camera is
not an input. Unsupported steps wait3seconds before another choice.

Existing straight supported dry-route checks and a bounded static-seep exclusion
admit targets. This is not broad pathfinding, obstacle-body collision, remote
knowledge, long-range wandering or a new activation framework. Needs/rest and
contextual destinations displace exploration; body steering immediately overrides
it. A new hero gets a fresh exploration controller. Original saved mode is off;
other existing diagnostic fixtures suppress exploration explicitly.

Run-ConnectedWorld.ps1 supports ExplorationProbe, ExplorationOverrideProbe and
ExplorationCameraProbe. These label a bank/noon startup; override mode supplies
body input at physical4..5, thirst at10 and a synthetic supported contextual goal
at15. Camera mode starts detached20m away. Real ordinary mode uses the original
start, calendar, needs rates and day/night. Native input fixtures are not human
keyboard/mouse acceptance. Use Duration24 for all three scheduled captures;
wall-clock19 runs can finish before physical17 because existing dt is capped.

The pause uses the existing idle body and a readable cue, with no bespoke head
look animation. Small repeated paths, simple turns, lack of broader obstacle
routing and inherited slow animal pace/contact remain draft limitations. No full
visual/30FPS/connected-save acceptance. Working role is enough: move onward.
