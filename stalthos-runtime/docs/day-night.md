# Connected-world day/night draft

The connected native host maps the existing Conditions.dayFraction to a proposed
sun path, warmer low light, cloud-dimmed daylight, sky exposure and a readable
blue night fill. Midnight=0, dawn=.25, noon=.5, dusk=.75. This changes rendering
only. The original saved populated mode keeps its existing lighting. No clock,
calendar scale, age, needs, rest, motion, resource or save schema changes.

Tuning lives in domain/day_night.hpp: nightAmbient, nightFill, nightSkyExposure;
trajectory and twilight blend are deliberately simple approximations. The first
native directional light remains the sun and controls Wicked's realistic sky.
Its transform uses the engine's +Y direction convention; measured light/sky
alignment is recorded in day-night.json. The second directional fill has no
shadows and is explicitly visibility assistance, not a simulated moon.

Run ordinary connected mode through scripts/Run-ConnectedWorld.ps1. A new world
starts at its existing calendar zero, now visibly midnight; no hidden noon reset.
Run scripts/Run.ps1 -DayNightPreview -Duration 16 for four labelled renderer-only
morning/noon/evening/night views. This does not fast-forward a day. Optional
existing Creature/CreatureAnimationMetadata and Plant arguments are supported.
-NightControlsProbe sets diagnostic fatigue and injects the ordinary reverse
body-control path for physical seconds2..4; it labels that synthetic setup.
Both diagnostics reject interactive/other diagnostic combinations. Ordinary
connected play remains interactive through the existing wrapper.

Acceptance requires final native same-place views, readable night body controls,
rest override/recovery, normal calendar phase equality and actual light/sky
alignment. Unit checks exercise trajectory, wrap, cloud modulation and clock/
needs separation. A preview is not elapsed24h, astronomy, human input, final art,
full-draft completion or30FPS evidence. Existing terrain/sole/wetness/rain and
resource limitations remain parked. Night fill is directional/unshadowed;
black sky, simple twilight and rough ambient/material response remain oddities
once movement is readable. Do not polish this subsystem after its role works.
