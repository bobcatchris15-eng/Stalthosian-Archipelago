# Smalltrotter nest and incubation working slice

Proposed fictional draft, October 22, 2026. In connected play, **B** explicitly
chooses the ground beside the animal and lays one clutch if the living adult
Smalltrotter is reasonably fed, hydrated and rested. The ordinary HUD explains
availability or refusal. Walking, turning, drinking, foraging and resting remain
immediate; nesting does not take locomotion control or lock the animal in place.
There is no special camera permission, parenting timer or mandatory nest vigil.

This is **partial reproduction**. There is no mating, actual hatch, offspring
admission, mortality or connected save. Three visible tapered eggs sit in a
replaceable earthen/twig nest. Incubation uses the same calendar step as aging,
weather and native populations, with a proposed 900 calendar seconds duration.
The end cue says explicitly that hatching is not implemented. No successful
birth, demographic increase, generated child identity or family is claimed.
The one clutch stays at its world location when the animal leaves, dies or a
new life starts in the running session. Returning within 12 m shows progress.

Placement is deliberately conservative: the original 192 m reference patch,
resident rendered ground at nine footprint points, <=0.18 m height relief,
dry creek exclusion, enlarged authored plant/rock/tree footprints and 3 m live
NPC spacing. This is a bounded clear-site approximation, not general collision
or nest habitat suitability. Exterior nesting waits for equivalent authored
obstacle coverage. Eggs have proposed dimensions and speculative anatomy.

`domain/nesting.hpp` contains eligibility and bounded calendar progression.
`engine/nest_scene.hpp` supplies replaceable visible geometry; no new art
pipeline is required. Neither changes identity/resource/session allocation.
Connected save/load refusal and original saved mode remain in their existing
owners. One session clutch is the explicit memory cap.

Focused verification:

```powershell
cmake -S stalthos-runtime -B var/build/nesting-domain -DSTALTHOS_BUILD_WICKED_HOST=OFF
cmake --build var/build/nesting-domain --config Release --target Stalthos_nesting
ctest --test-dir var/build/nesting-domain -C Release -R '^domain-nesting$' --output-on-failure
```

Native diagnostic after an integrated build:

```powershell
stalthos-runtime/scripts/Run.ps1 -NestProbe -Duration 14 -CalendarScale 100
```

The diagnostic alone restores an adult and searches a clear starting pose. It
injects the same B intention at 2 physical seconds and ordinary reverse input
at 4–7 seconds, with noon presentation. The shared calendar still advances all
systems normally. It captures composed frames at 3, 8 and 12 seconds and writes
`nesting.json` with eligibility, position, input, incubation, movement and
explicit zero offspring. Use an ordinary interactive connected run to exercise
the real B key after the animal reaches adulthood. Domain verification does
not establish the native visual result or 30 FPS acceptance.

Next bounded task: add actual independent Smalltrotter hatchling admission.
First inspect `native_population_registry.hpp`, region population caps, unique
world IDs and fresh interaction sessions, and `fauna_scene.hpp` actor loading.
Admit a bounded clutch through the live authoritative population only once,
close the egg state after successful admission, and give each actual newborn
the ordinary needs/lifecycle/graph locomotion with hatchling bulk. Keep eggs
when admission is blocked, never silently invent success or replace existing
actors. Prove no duplicated IDs/sessions or ledger reset, retained new actors
after body departure/return and next life, visible shell-to-independent-young
transition, and that Smalltrotter hatchlings require no prolonged parent care.
