# Task brief: native animated actor and lifecycle host

Base revision: `f72f3b5b20de8bb40a8d4b7310592c745dee0a3c`.

Scope: `stalthos-runtime/engine/main.cpp`, `stalthos-runtime/scripts/Run.ps1`, new `stalthos-runtime/docs/actor-host.md`, and this task record. Implement an optional GLB actor imported into a fresh Wicked scene, validate mesh/skin/TRS animation, attach the imported scene root to an actor motion parent, loop and play/pause animation from walking input without resetting imported rig transforms. Wire `Lifecycle` to the `SimCore` `TimeStep`, retain backward-compatible age-zero fallback, save/restore current hero lifecycle when available, stop motion and call `heroDied()` at terminal death. Add `-Creature`/`-CalendarScale`, phase screenshots, and measured timer/local-channel evidence. Preserve the proxy fallback and clearly state asset limitations. Do not modify planning input or commit/push.
