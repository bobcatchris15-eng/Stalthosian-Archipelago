# Authored exterior cove draft

The connected preview has one bounded authored cove beyond the current meadow.
This is a local coastal layout approximation, not a claim that the coarse island
coast kilometres away has been connected or that all island geography is final.

ExteriorShoreLayout in domain/exterior_shore.hpp owns the editable descent start
x200, profile endx280, centrez24, zero sea datum, submerged depth2m, half-width32m,
flare.4 and side-bank blend64m. ConnectedRoute preserves its previous heights
through x200 and blends the new bed into the old exterior outside the cove.
The normal streamed two-metre triangles remain the body-ground authority.

A finite static water sheet at zero is occluded by dry terrain. Fourteen existing
organic-rock meshes mark its margins; low ground becomes pale dry sand or darker
wet-bank color. Existing textures and rock generators remain replaceable. The
water sheet ends atx800/z-400..448; no hydrology, tides, salinity, swimming, boat,
new drinking resource, physical rock collision or ocean horizon acceptance.

The meadow resources208/209, existing woodland identities and all earlier ground
at x<=200 retain their prior locations/heights. Physical body position remains the
streaming/population driver. Camera movement does not grant remote resources or
HeroSim knowledge. Original saved mode uses the previous terrain path; connected
saves remain explicitly unsupported.

## Native diagnostic controls

- Run-ConnectedWorld.ps1 -Proxy -ShoreRouteProbe: labelled local x184/z24 start,
  ordinary forward integrator through the descent to the dry target nearx258.
- -ShoreEntryProbe: existing animated animal starts locally atx254 and takes the
  same ordinary supported controls towardx258 at its measured existing pace.
- -ShoreStillProbe and -ShoreCameraProbe: equal fixed body startx184, attached
  versus detached/moving cove camera. These compare bodies/activation/knowledge.

All fixtures use a labelled synthetic noon renderer phase and normal physical/
calendar progression. A proxy descent or local animated entry is distinct from
an animated full watershed-to-coast route. Relevant images, measured support and
verification scope belong to the current task receipt. Build/tests alone do not
establish recognizability, foot contact, full-game visuals or30FPS.
