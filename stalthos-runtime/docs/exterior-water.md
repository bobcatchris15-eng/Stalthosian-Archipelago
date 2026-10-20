# Exterior meadow water source draft

Connected preview adds a small static rock-lined seep with resource209 (quantity2).
Dry bank is the existing resident node132,30; water center132,33.5/radius2.6.
The water plane sits0.04m above maximum sampled local terrain; a low irregular
stone rim joins the surrounding ground. Existing Wicked water material renders
the flat surface. This is a static perched shallow seep/basin approximation,
not terrain excavation, springs/hydrology, fluid displacement or a water solver.

ExteriorWaterSource holds the tuning. The dry bank must remain within0.85m
vertically of the source. Hero contact requires resident rendered body support,
<=1.1m horizontal reach of the dry bank and bounded vertical reach. The existing
single-goal drink controller chooses the nearest body-local eligible source;
it now passes the selected resource ID through shared contact/relief. The
original creek100 is preserved. Direct body controls cancel normally; the camera
grants no body water awareness or remote contact.

New exterior resident index10 starts thirsty at that bank, through the same
registry/contact history. The original eight actors/creek resources remain;
fern resource208 and the last browser remain. Existing coarse192m activation
and retained support tile(1,0) stay. Quantity is finite and water is hidden when
empty; there is no gradual level change, refill or regrowth scheduling. Collision,
wading, detailed drinking pose and bank/rock art are provisional. No lethal
ordinary drinking or new save format. Connected preview still refuses saves.

Run.ps1 -ExteriorWaterProbe uses labelled synthetic initial thirst and proxy
16m east of the dry bank; motion/contact are ordinary. -ExteriorWaterCameraProbe
starts28m away with the camera on the source; NPC feeding/drinking may continue,
but the hero must not gain awareness, move or drink. -ExteriorWaterOverrideProbe
adds direct reverse input during physical seconds2–4. No post-start teleport.
Native exterior-water JSON records shared NPC/hero contacts, relief and remaining
quantity. Build alone is not visual/full-draft/30FPS acceptance.

The original100-only water admission check needed one concrete integration
exception:209 is accepted as water only when connectedPreview=true. Original
saved-mode defaults still reject that declaration; no save format/configuration
or migration changed. The focused test verifies both admission paths.
