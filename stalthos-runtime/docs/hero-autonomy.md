# Ordinary hero autonomy working draft

Normal ConnectedWorld play now lets an uncommanded animal act on its existing
needs suggestion. This bounded first action is **drink**: when thirst is the most
urgent supported need, a body-local creek observation can become a HeroSim goal.
The animal turns at a bounded rate, approaches with the same forward-intent2m/s
movement integrator used by ordinary controls, stops at the existing water
eligibility, and uses the existing interaction/session/registry authority to drink.
No body teleport, alternate ledger or resource grant exists.

W/S/A/D/E take direct authority immediately. Camera-only movement supplies no
observation to the animal. Body observations are bounded to24m and the authored
watershed. An interrupted autonomy goal is cancelled; manual E receives a fresh
interaction owner/session boundary if it begins during an autonomous contact.
One accepted autonomous contact completes that bout, then a short cooldown
prevents immediate retriggering. Existing fresh needs values remain unchanged.

```powershell
.\stalthos-runtime\scripts\Run.ps1 -ConnectedWorld -Interactive -CameraDistance 10
```

Bounded native probes explicitly seed thirst0.8 while exercising the same ordinary
choice/movement/contact logic:

```powershell
.\stalthos-runtime\scripts\Run.ps1 -HeroAutonomyProbe -Duration 25 -CameraDistance 10
.\stalthos-runtime\scripts\Run.ps1 -HeroAutonomyOverrideProbe -Duration 25 -CameraDistance 10
.\stalthos-runtime\scripts\Run.ps1 -HeroAutonomyCameraProbe -Duration 6 -CameraDistance 10
```

The override probe injects ordinary reverse input during seconds2–4, then releases
it. The camera-only probe starts the body at(40,24), outside water sensing range,
and initializes a detached camera toward the creek. These startup/input setups
are synthetic; autonomous approach and consumption are real. The camera probe
never relocates the body during its trial. hero-autonomy.json records decisions,
authority, body/target/camera positions, contacts, exact thirst relief/session and
captures. The existing world-population.json carries the complete shared ledger
including simultaneous NPC consumption, so hero resource use must be compared
by its own session rather than attributing the whole creek delta to the hero.

Logged scope: drink only, no general pathfinder, forage or rest implementation;
no remote resource knowledge; no needs retuning, new persistence, animation or
anatomical mouth-to-water acceptance. Existing water eligibility can lead to a
wading/submerged proxy because the reference creek is deeper than its proxy body;
this draft does not redesign reach/swimming. The existing connected preview save
refusal and terrain/weather oddities remain. Original non-connected modes retain
their earlier player and diagnostic behavior.
