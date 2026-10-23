# Body-local potential-danger cue and interruptible recall

Working draft: the actual hero motion root senses an existing alive adult or old
Longstride body within 12 metres in 3D. A continuous encounter retains its cue
until that body leaves 16 metres; a nearer eligible actor may replace the named
identity without restarting recall. Only physical population views are eligible.
No camera position, attack inference, damage, pursuit, or new animal is involved.
This proximity approximation does not model occlusion or hearing.

The composed UI says `Longstride nearby - potential danger`. If the camera is
already detached when the encounter starts, the existing HeroViewController
smoothly recalls its anchor to the body and switches to ordinary follow within
0.03 metres. The camera keeps its orbit and framing/tether/terrain checks.
Camera pan, orbit, wheel, F1, or V inspection action immediately cancels recall.
An encounter requests recall once; cancellation remains effective until genuine
separation and another encounter. Ordinary needs, autonomy, body control and
contextual travel continue through their existing owners. This session-only cue
and recall do not change saves or HeroSim resource knowledge.

## Bounded verification

`domain-carnivore_awareness` covers retained identity, adult eligibility,
herbivore/dead/self exclusion, vertical and horizontal separation, remote body,
inactive population, recall completion, explicit input cancellation and camera
transaction rollback. Existing `domain-hero_view_controller` also passes.

From the main checkout after integration/build, run:

```powershell
./stalthos-runtime/scripts/Run.ps1 -CarnivoreProbe -Duration 25
./stalthos-runtime/scripts/Run.ps1 -CarnivoreCancelProbe -Duration 25
./stalthos-runtime/scripts/Run.ps1 -CarnivoreCameraProbe -Duration 25
```

These are labelled diagnostic fixtures: synthetic body startup 8 metres from an
existing eligible retained actor (28 metres for camera-only), detached camera
startup, reverse input from physical seconds 6 to 20, and an injected orbit input
at 0.5 seconds for cancellation. They do not relocate or duplicate the actor.
A bounded dry local retreat line is selected; no global route claim follows.
Existing species proxies remain replaceable. Native reports are
`carnivore-awareness.json` with at most 2400 body/actor/camera/needs samples and
three composed-backbuffer screenshots including the UI.

Acceptance gates: body fixture has retained actor/cue then actual body separation
and no cue; one recall request converges to follow. Cancel fixture has recall
before 0.5 seconds, detached camera afterward, no repeat request during the same
encounter, and actual separation clears cue. Remote camera fixture shows its
camera near the watched actor while the body stays remote, zero cue/recall.
Needs continue changing and reverse movement continues during these fixtures.
Native execution and screenshot inspection must be reported separately from the
focused domain-test evidence; synthetic input never proves human-input testing.
