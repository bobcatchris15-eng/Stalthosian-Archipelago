# Play the connected working draft

Double-click **Play.cmd** in this folder, or run `pwsh -NoProfile -File .\Play.ps1`.
The game stays open until you close its window or press **Esc**. PowerShell 7 and
the locally built Windows executable are required. The launcher uses the existing
pinned animal, fern and woodland assets when available; missing assets are announced.

Choose the contrasting implemented bird life with:

```powershell
pwsh -NoProfile -File .\Play.ps1 -HeroSpecies Thistletail
```

The default remains `Smalltrotter`. The four implemented choices are `Smalltrotter`, `Thistletail`, `Brackenback` and `Reedcrest`.
Thistletail uses a replaceable feather-layered procedural biped with its own
selected needs/lifecycle, ordinary controls and next life. It can forage a finite
litter/seed cohort near24,24 through the same food ledger and ordinary needs
intention. No individual insects, bird breeding/parenting or finished plumage art
is claimed. Imported Smalltrotter meshes, when available, remain NPC presentation.


Choose the armored quadruped with `pwsh -NoProfile -File .\Play.ps1 -HeroSpecies Brackenback`.
Its actual needs/lifecycle and ordinary controls use the same world. Mature
Brackenback may briefly interpose for a nearby critically hungry/thirsty small
animal when a live adult Longstride is close. A visible condition cue identifies
the distress approximation; no attack intent, damage or adoption is implied.
Only the hero response is integrated; adult NPC protection remains missing.
The tail club appears at adult/old stages. Geometry and gait are replaceable draft art.

Choose the beaked basal hadrosauroid with `pwsh -NoProfile -File .\Play.ps1 -HeroSpecies Reedcrest`.
A calm local encounter can form one session peer link with an actual retained
Reedcrest. Either body may lead; the other follows or waits as ordinary needs
permit. The pair does not teleport. This is one reciprocal pair, not a full
kindergarten/cohort or connected-save implementation. Hero geometry is replaceable;
NPC species art remains provisional.

| Input | Action |
|---|---|
| W / S | Move forward / reverse relative to the animal |
| A / D | Turn the animal |
| F1 | Detach the camera and give it keyboard focus; press again to return |
| W / A / S / D while detached | Pan the camera within its body tether |
| Left mouse drag / wheel | Orbit / zoom |
| V | Toggle close inspection framing |
| Right-click nearby clear dry ground | Ask the animal to travel there; direct steering cancels |
| E | Attempt an eligible nearby resource interaction |
| F with animal control | Disturb the eligible nearby sapling |
| B with animal control | Adult Smalltrotter: attempt to lay one clutch on nearby dry clear ground |
| N after death | Begin the next life in the current session |
| Esc | Exit |

Let go of the controls to observe ordinary autonomous activity, including food,
water, rest and short exploration. The connected draft includes the watershed,
pass, meadow, woodland and authored cove, with retained local animals and finite
shared food and water. The existing animal moves slowly; its anatomy and animations
remain provisional. The calendar starts at midnight, so the initial scene is dark.

**Connected sessions currently start fresh and do not support saving or loading.**

An adult can place one visible nest with three eggs; incubation follows the world
calendar. One egg can produce a real independent Smalltrotter with its own needs,
age and movement. One egg becomes a shell; the other two currently remain inactive.
This is a bounded reproduction draft: mating and other species' parental care
remain unfinished. Nearby adult Longstride bodies can cause a potential-danger cue and recall a
detached camera. Intentional camera input cancels recall; proximity alone does not
mean the Longstride intends to attack.

The original watershed save mode remains available through the existing runtime
scripts. This connected draft does not yet deliver all five playable species,
family/reproduction gameplay, four traversable islands or verified target-hardware
30 FPS. Current demonstrated scope and limits are recorded in
[the coverage ledger](.clanker/DRAFT_COVERAGE.md).

For a bounded normal launch using the same assets and world, run
`pwsh -NoProfile -File .\Play.ps1 -Duration 15`. Use `-WhatIf` to inspect the launch
without opening a window, or `-Proxy` for the explicit diagnostic body fallback.
Run from any working directory by supplying the absolute path to Play.ps1.

If the executable is missing, build from this folder with
`pwsh -NoProfile -File .\stalthos-runtime\scripts\Build.ps1`.
That requires the pinned Wicked source, verified local compiled dependency cache,
CMake and Visual Studio C++ build tools described in the runtime documentation.
The play launcher does not download or rebuild dependencies.
