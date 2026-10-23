# Play the connected working draft

Double-click **Play.cmd** in this folder, or run `pwsh -NoProfile -File .\Play.ps1`.
The game stays open until you close its window or press **Esc**. PowerShell 7 and
the locally built Windows executable are required. The launcher uses the existing
pinned animal, fern and woodland assets when available; missing assets are announced.

Choose the contrasting implemented bird life with:

```powershell
pwsh -NoProfile -File .\Play.ps1 -HeroSpecies Thistletail
```

The default remains `Smalltrotter`. The five implemented choices are `Smalltrotter`, `Thistletail`, `Brackenback`, `Reedcrest` and `Longstride`.
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

Choose the bipedal carnivore with `pwsh -NoProfile -File .\Play.ps1 -HeroSpecies Longstride`.
Its separate theropod body owns ordinary needs/lifecycle/control and same-species
next life. One finite authored carrion cohort near24,30 offers real body-local
scavenging and visible depletion. The prop is not an actual dead NPC; hunting,
NPC carrion feeding and dependent-young care remain missing. Five selectable
working lives do not establish complete species histories or the full game.

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

Adult Smalltrotter B now invites an actual nearby calm adult conspecific. Its physical
approach and quiet readiness precede clutch admission; no mate or interrupted
response means no clutch. Direct input, a player destination or ordinary needs
cancel the temporary courtship. This is a speculative paired-readiness draft,
not complete reproductive biology or other-species breeding.


### Partial connected persistence profile (separate STLC1 entry)

Normal connected Play remains a fresh session. Opt in explicitly to a deliberately
limited persistent profile:

```powershell
pwsh -File .\Play.ps1 -PartialPersistence -HeroSpecies Longstride -ConnectedSave 'E:/Stalthosian Archipelago/var/saves/my-partial.stlc'
pwsh -File .\Play.ps1 -ConnectedLoad 'E:/Stalthosian Archipelago/var/saves/my-partial.stlc' -ConnectedSave 'E:/Stalthosian Archipelago/var/saves/my-next-partial.stlc'
```

Read the entry banner: this profile keeps WASD movement, E body-local feeding/drinking,
R toggle rest, nearby ground destinations, camera controls, actual needs/lifecycle,
real NPC simulation and N after death. It disables automatic hero needs/exploration,
peer learning, protection and courtship from entry. Those behaviors remain in normal
Play. It cannot import an ordinary connected session or its nests/bonds.

F5 writes a checkpoint to the selected ConnectedSave path. Release movement/E and
finish or cancel a destination/recall first. A refusal appears in the HUD and keeps
the running world and existing save. Exit also attempts a checkpoint; check its
report if exiting during an unsupported action. Reload by restarting with
ConnectedLoad. Loading selects the saved species and calendar scale; an explicit
conflicting species is refused. Saves need a living hero; after death use N first.

STLC1 preserves clocks, live body species/identity/pose/needs/lifecycle, live and dead
NPC snapshots and bindings, prior hero death truth, resource quantities and full
contact histories, current/next sessions, wetness, camera focus/anchor/orbit and tree
exceptions. It validates the generated roster, resource configuration, seed, habitat,
asset bytes and configured motion values. Changed imports/stride/sole offset and
other incompatible configurations refuse. It never overwrites an original STL8/9
save. This is partial persistence, not full connected-save or reproduction coverage.


Young Longstride normal fresh lives now begin at a supported local birth position
near one actual retained adult wild caregiver. This is a bounded wild-care association,
not proven biological lineage. Idle young follow that caregiver's actual body when
nearby; WASD, a player destination and own needs override. The adult keeps its own
NPC route and needs. Separation never selects a replacement; caregiver death/loss or
maturity ends care. N discards the prior young life's association. Smalltrotter young
remain independent. Partial persistence excludes this normal-mode family state.
