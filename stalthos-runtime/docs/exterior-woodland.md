# Exterior woodland working draft

One bounded leafy stand adjoins the exterior meadow at x148..184,z38..56.
Six larger trees and four saplings use the existing prototype importer and shared
mesh/material resources. The existing connected launcher now pins both GLBs;
missing files warn and omit that stage, changed hashes fail before launch.
Existing corrected-crown parent exports are broad-foliage stand-ins. They are
provisional fictional assets, not stage/anatomy/final-art acceptance. Needle-spray
parent exports remain unchanged: their first native view read as sparse specks,
so no foliage repair or generation was undertaken. Both sets are hash-verified.

Edit domain/exterior_woodland.hpp to tune the ten positions and lane. The central
supported lane runs x146..184,z47. Existing food144,20 and water132,33.5 stay south
of the stand. Importer samples the same resident streamed rendered triangles as
body support and applies the existing measured model-base correction. The small
stand lies within the existing retained support tile(1,0); no terrain/activation
framework changes. Tree identities9000..9009 are render placement identifiers,
not newly registered fauna or resource identities. Original nonconnected woodland
proposal generation remains the default path. No camera-driven knowledge exists
for this static placement; no claim of canopy weather shelter or collision system.

Ordinary play: scripts/Run-ConnectedWorld.ps1 -Interactive. Existing calendar zero
is midnight and remains unchanged. Native diagnostic: same script -WoodlandProbe
-Duration24 -Proxy -CameraDistance18; omit Proxy for the existing animated body
and use Duration32/CameraDistance8 for bounded actual-animal entry. The diagnostic
starts near the stand, injects ordinary forward intent until x184, and previews
noon rendering only. The screen and day-night.json label synthetic setup. It does
not advance clocks, needs, ages or resources artificially. Captures reuse the
existing day-night-0..3.png diagnostic filenames. Normal launch has no relocation,
input injection or phase override. The original save-mode launch still works;
connected preview continues to refuse save/load.

Final native proxy traversal38.0058m; actual animated animal entry4.67145m. Both
have zero support failures/fallbacks. Ten trees placed, none skipped;4 unique
objects,7616 unique triangles,36840 drawn triangles from shared prototypes. These
are geometry counts, not FPS measurements. Original12 actors and11-resource/
session initial state exactly match the prior ordinary run. Release and50domain
checks pass; see task receipt for toolkit and exact native evidence.

Parked oddities: sparse crowns, broad coarse blades and mostly open canopy;
straight rows are a replaceable layout; rough sandy ground/groundcover and body
sole/contact remain. Trunks have existing scene ray intersections but no new body
collision contract. No physical canopy shelter, forest ecology, all-island forest,
full animal lane crossing, human-input/FPS/final-art/full-draft claim. Role works:
recognizable bounded trees/saplings and supported entry. Move to next subsystem.
