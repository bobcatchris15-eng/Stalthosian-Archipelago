# Splitcrown reference audit — provisional assets

Read-only source audit by AGY low, frozen6b234c4, produced real Blender5.2
sapling/young_adult sources and four-angle turntables in ignored workspace:
var/worktrees/splitcrown-reference/var/splitcrown-reference-20261013.
Generation and rendering commands exited0. Parent viewed young_adult090.

## Stage sizing issue

Requested sapling authored height is4.2m; young_adult14m. Both actual generated
trunks are approximately26m, with overall foliage height28.24m/28.06m. Each
source has5502vertices/5460triangles and two materials. The stage payload is
present but its growth scales are not consumed by canonical source generation.
Jules8336722969998514867 is assigned the source fix. Do not conceal this with
runtime instance scaling or treat these assets as correctly sized young stages.
Crown spread/morphology and late leader fork remain provisional, independently
of the sizing correction. The viewed foliage uses coarse untextured spray fans
and bark uses a flat material; reference visual acceptance is open.

## Export/LOD evidence and limits

Existing generate_lods strict run exited2: gltfpack exceeded shared budgets.
The explicit allow-degraded run exited0 with unmeasured Python hard-cap warnings.
Intermediate source/lod0 GLB is248032bytes, SHA256
`af3b272d53e24508e30c4c3c9b5bd40f413fe1f0468c74dbd069bc939d7302bf`.
This export is a provisional mature-sized diagnostic prototype. Recorded lower
LODs have2182/803/264triangles; visual simplification quality is unmeasured.
The two stages shared an asset manifest, so its later version supersedes earlier
manifest hashes. Preserve the worker's original receipts and sources; use
separate fresh stage workspaces for corrected validation.

Toolkit code was unchanged by this audit. Actual source generation/rendering is
verified operational evidence, not native woodland, stage-correct geometry,
artistic, performance or full draft acceptance. Reviewed woodland placement
helper is separate; host import/placement and matched captures remain pending.
