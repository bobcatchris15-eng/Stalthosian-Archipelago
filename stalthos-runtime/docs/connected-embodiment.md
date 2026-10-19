# Connected animal embodiment working draft

Reuse of the reviewed Smalltrotter GLB makes the connected playable body an animal
through the existing native importer, animation/root translation removal, crossfade,
measured pace and body-ground adapter. No meshes, materials, rigs or gaits changed.
The original bare Run.ps1 proxy behavior remains available, and original save modes
are unaffected. Connected preview continues to refuse save/load.

```powershell
.\stalthos-runtime\scripts\Run-ConnectedWorld.ps1 -Interactive
.\stalthos-runtime\scripts\Run-ConnectedWorld.ps1 -Proxy -Interactive
.\stalthos-runtime\scripts\Run-ConnectedWorld.ps1 -AutonomyProbe -Duration 25
.\stalthos-runtime\scripts\Run-ConnectedWorld.ps1 -ExteriorProbe -Duration 12
```

The convenience launcher reads config/connected-creature.json, verifies both
reviewed local artifact hashes, supplies the existing metadata and SoleOffset0.
Missing assets produce an announced proxy fallback; changed assets fail rather
than pretending they are the pinned review. AssetRoot may identify another local
checkout holding these read-only var artifacts. No download/build is performed.
The named receipt is agy-bristle-validation-resume/receipt.json; the matching GLB
and JSON hashes also occur in the parentverified rig artifact manifest.

Existing LocomotionPace uses measured stride0.16128m at one cycle/second, yielding
0.16128m/s. The authored duration2s remains unchanged. A full176m animal route
crossing cannot fit the native120s diagnostic cap; no faster proxy pace is applied
to fake that proof. The autonomy model probe explicitly starts at(10.5,24), near
water, and seeds the existing need fixture. The exterior model probe starts at
(104,24), on the exterior rendered mesh, then walks through the normal integrator.
These are startup fixtures, not teleports during play or proof of full-route
crossing by the model. Normal ConnectedWorld spawn/controls remain unchanged.

Known limits: the current source is an independent-juvenile model while existing
hero lifecycle stage selection is unchanged. No anatomical contact, gait quality,
foot locking, water/swimming or final art acceptance. The four-foot/sole diagnostic
is still reference-fixture bounded and becomes unresolved outside it; rendered
body/root support continues through PopulatedTerrain. Proxy-only bounds metrics
are explicitly inapplicable to imported hierarchy roots. Wet-body material tint
is proxy-only to preserve the imported authored material hierarchy; material
wetting is not ported to the GLB. Asset/source geometry remains replaceable.
