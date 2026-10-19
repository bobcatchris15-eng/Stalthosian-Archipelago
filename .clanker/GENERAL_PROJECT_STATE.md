# Current world construction checkpoint - 2026-10-18

WORLD WORKING AND REFINABLE. User completion rule: when a subsystem draft works,
call it good, log oddities and move to NEXT SUBSYSTEM. Broad connected coverage
outranks polish/exhaustive edgecases. Wicked Engine; assets remain replaceable.
Read .clanker/DRAFT_COVERAGE.md and current next-task, not old transcripts/audits.

## Working drafts accepted: connected route and weather/body wetting

ConnectedWorld is the normal populated host with a drybank spawn, real rendered
pass and exterior dry-grass/outcrop meadow. Final95s native body proof24->202.0458m
at ordinary2m/s crossed96m watershed and128m streamed joins.15970samples match
speed*dt within7.63e-6m; emitted-ground support/seams passed, final passage-facing
player captures inspected. No teleported body or camera-only traversal claim.
Layout settings drive route/landmarks/proof; original192m terrain preserved.

Weather uses existing calendar conditions for overcast light/wind and physicaldt
for visibly darker/smoother wetbody. Final12s native demonstration exited0 with
3captures and38samples, wetness0->.661649 and physical/calendar clocks unchanged.
Independent review accepts both working drafts. Final native Release build passed;
CTest40/40 passed2.48s. Toolkit2073passed2skipped620.54s; no toolkit sources changed.

Use: .\stalthos-runtime\scripts\Run.ps1 -ConnectedWorld -Interactive -CameraDistance 10
Route receipt: .clanker/tasks/connected-world-route-20261018/integration-receipt.json
Weather receipt: .clanker/tasks/weather-response-20261018/integration-receipt.json
Reviews: connected-world-route-review-20261018 and weather-response-review-20261018.

## Logged oddities; move on rather than polish these now

Route sparse/proxy bodies, provisional rocks, foot offset on slopes, steep far
flanks and no authored eastern end/island geography. No exterior retained fauna.
Connected preview refuses save/load; existing original-watershed STL9 mode remains.
Native rain emitter crashed on transition and is disabled in connected mode.
Rainfall remains wetting input: NO visible rainfall claim. Overcast is lighting,
not volumetric clouds; shelter/comfort is wind-only approximate telemetry, not
roof occlusion, needs/damage coupling or proven shelter navigation. Synthetic
weather/shelter phases are explicit. No final art/contact/full-draft/FPS acceptance.

## Working draft accepted: ordinary hero drink activity

Integrated e02ea31. Body-local24m water awareness and existing need suggestion
produce bounded turn/ordinary2m/s proxy approach/one eligible shared-ledger drink.
Final integrated native approach consumed.5 and relieved thirst.5; total creek
change matches hero+NPC consumption within4.44e-15. Independent frozen-source
native direct-input and camera-only trials passed; no camera resource grant.
Final Release host build uses required /bigobj; CTest41/41 passed2.81s. Toolkit
2073passed2skipped543.56s (unchanged toolkit source). Drink-only, radial-awareness,
submerged proxy and missing forage/rest/generalpathing are logged, not polish work.
Receipt: .clanker/tasks/hero-autonomy-20261018/integration-receipt.json
Review: .clanker/tasks/hero-autonomy-review-20261018/independent-review.json

## Working draft accepted: connected animal embodiment

Integrated 07ab1ab reuses the pinned existing Smalltrotter GLB/metadata, native
importer, animation/crossfade, measured pace and body-ground adapter. No asset,
gait, anatomy, authored material or save-format changes. Run-ConnectedWorld.ps1
verifies local artifact hashes; missing artifacts announce proxy fallback.
Worker frozen-source Release and independent native review passed. Exterior12s
startup fixture104->105.6601m, changing bone poses, support error<4.8e-7m and
zero rendered-ground fallbacks. Nearwater25s fixture produced one .5 drink and
exact .5 thirst relief. Captures of actual animal on exterior and water inspected.
Root final Release build passed; CTest41/41 passed3.61s. Final exterior12s2168samples
104->105.6261m/supporterror<4.8e-7m/fallback0 and actualcapture inspected. Final
embodied25s contact/ledger/relief also passed. Final native build/proof receipt: tasks/connected-embodiment-20261018/integration-receipt.json.

Logged: measured .16128m/s is slow; no full-route animal crossing claimed. The
juvenile mesh/lifecycle mismatch, submerged water presentation, unresolved exterior
sole diagnostics and proxy-only wet material response remain rough draft limits.
No full-art/anatomical-foot/full-draft/30FPS acceptance. Do not polish these now.

## NEXT SUBSYSTEM: nearby wildlife awareness and avoidance

Cold brief: .clanker/tasks/connected-embodiment-20261018/next-task.md
Give nearby retained animals a visible bounded reaction to the actual hero body,
using existing movement/support and preserving IDs/resource state. Camera-only
movement must not trigger a body encounter. Reuse replaceable visuals, tune radii
and response simply, prove approach/withdrawal in native view. No generalized AI,
asset-polish or save-format prerequisite. Collect existing workers before dispatch.

Preservation: prior untracked hashes retained except authorized graph cache marker;
no source/planning/apps/history cleanup. Two local workers reused sequentially,
peak2; four bounded implementation/review assignments total, no AGY/Jules. Explicit
owned-path checkpoint, secret scan and reviewed origin main publication authorized;
no force push/releases/deployment/paid APIs/credentials. Historical receipts remain.
