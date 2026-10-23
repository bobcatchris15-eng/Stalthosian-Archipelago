# Shared four-landform native draft and first physical crossing

NativeArchipelago reuses Archipelago continuous drowned geological field, at editable
first-draft horizontal/vertical scales0.1. Existing reference watershed and habitat/
resource coordinates and east route are preserved in[-96,300]x[-96,96], blending over
192m into the common field. Rainridge/Stone/Outer centres and full coast-neighborhood
geometry use that same field; no separate disks. Pinned128m terrain neighborhoods
around all four masses plus body streaming use the same2m triangles as body queries.
Bounded sea sheets exclude the original freshwater reference square. Simple vertex
palette differences are provisional; no new island ecology or final scenery claim.

Body-only land identity returns Main/Rainridge/Stone/Outer on dry terrain and no-island
at sea. Original reference freshwater remains Main. Existing foundation hero.region
and watershed registry ownership intentionally stay legacy; full region migration/
new island populations remain a gap. Camera geometry cannot change body island or
NPC activation. The first diagnostic starts on actual Main shore and holds ordinary
forward input across about195.5m (191.5m sea) toward actual Stone dry shore. No
intermediate pose change, travel menu or teleport. A synthetic shore-start is labelled.

Marine body datum is max(actual rendered seabed,-0.55m), continuous through wading
into swimming, with existing controls/one clock/one Needs tick. Swimming limits speed
and adds physical fatigue, clearing rest before Needs advances. Camera clearance
uses the same static sea surface instead of diving to bed. Gait/buoyancy are replaceable
prototypes; no current, drowning, fully animated paddling or final anatomy claim.

```powershell
pwsh -File stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -IslandCrossingProbe -Duration 240 -AssetRoot 'E:/Stalthosian Archipelago'
pwsh -File stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -IslandCameraProbe -Duration 16 -AssetRoot 'E:/Stalthosian Archipelago'
```

Only this crossing diagnostic permits300s; ordinary diagnostics retain120s. Camera
fixture observes coastal sea while body stays on Main; it is not a remote-island
camera tour. island-crossing.json records body datum/seabed/depth/state/island/fatigue,
actual NPC activation, full ledger and composed captures. All-four traversal and native
first-crossing acceptance remain root-owned pending source-stable runtime proof.

Manual partial STLC config revision202610230400 intentionally refuses older layouts;
unsupported marine or extended-region save/load poses refuse before writing/swapping.
Old files are retained, no migration/reset. Source tests verify exact protected-origin
field, four land uplands, actual Main/sea/Stone route, freshwater identity, continuous
water datum, fatigue/rest behavior and padded streaming bounds. 30FPS unaccepted.


## Ordinary crossing and stance repair

The previous native failure was real: original dry start34.45deg exceeded the unchanged
20deg terrain-support cap; water seabed slopes also polluted the foot-failure counter.
The global protected-origin blend is now192m (protected coordinates/heights unchanged),
and the crossing search chooses a real shared-field shore route whose complete dry
and wading portions meet the same20deg cap at young and adult body spans. This is a
world geometry change, not a fixture-only lane. Route195.5m includes191.5m actualsea.

No ordinary stride, swimming speed, slowdown, needs, weather or exposure buff was
introduced. A focused exact-owner integration with pinned.16128m/s pace, real weather,
wetness/exposure, ordinary Needs and swimming predicts about197s to the endpoint
(first dry landing around194s), ending tired atfatigue1. These are domain predictions,
not native acceptance. Recommended native duration240/cap300 gives honest margin.

Dry and wading support failures are counted separately and remain real failures;
swimming is buoyant stance where foot support on the seabed is not applicable.
Raw seabed slope remains recorded. Supported waders retain the terrain-aligned pose;
unsupported waders retain the actual fallback/failure. The camera-only diagnostic now
captures a composed frame after its pan at5s. Explicit Longstride crossing diagnostics
skip the unrelated caregiver birth placement; normal Longstride births are unchanged.
STLC configuration400 rejects prior layouts without rewriting those files.
