# Independent watershed join review — 2026-10-18

Reviewer owns this report only. Review workspace: `var/worktrees/watershed-join-review-20261018`, baseline `02ef908` with coordinator host diff and verifier copied in. Root `.clanker/HALT` absent when checked. Read current-work and terrain next-task before review. No implementation files modified.

## Verdict

Domain overlay and optional host mode are suitable for bounded diagnostic integration, subject to the coordinator verifier/evidence follow-ups below. This does not accept production population traversal, final scenery or 30 FPS.

## Findings and required follow-ups

1. **P2 — capture verifier does not establish before/at/after coverage.** `Verify-WatershedJoin.ps1` accepts three capture flags and existing paths while ignoring `capture_diagnostics`, capture positions and renderer visibility. Timed capture thresholds can also produce several captures from one frame after a long stall. Empty files satisfy the path check. Coordinator acknowledged and will enforce three finite diagnostic records, ordered capture indices, visible objects, and spatial brackets (interior <=96m, outer join 230..300m, coarse >=350m), plus independent image inspection. This finding concerns initial supplied revision; final fix requires re-review.
2. **Evidence strengthening — read back engine-owned mesh arrays.** The initial stream probe creates support cache and digests from local submitted float arrays, rather than `mesh->vertex_positions` / `mesh->indices`. These arrays are currently equivalent at creation, so no demonstrated contact bug, but using engine-owned arrays strengthens the stated actual-buffer evidence. Coordinator acknowledged and plans this change.
3. **P3 — final route-return check can reject a completed timed run.** The loop breaks before sampling elapsed >= duration. At 12 seconds, return speed is 86.67m/s and x<26 allows only about 23ms before the endpoint. A final frame farther from the duration threshold can report false even when the route would return on its next sample. Consider explicitly sampling the endpoint or using measured final coordinate with a justified tolerance. This is a false rejection risk, not a demonstrated false acceptance.

## Checks and reasoning

- Independently invoked current coordinator-built Release `Stalthos_watershed_terrain.exe` and `Stalthos_terrain_tiles.exe`: both returned status success, groups 3. Reviewer did not independently rebuild these executables.
- Exact interior height path delegates directly to original reference sampler and preserves original coordinates/datum. Native 2m lattice and triangle winding match original `Terrain()` construction; float reference corner comparison is consistent.
- Composite finite/world bound checks precede arithmetic; configuration delegates to bounded coarse sampler; tile-key guards protect integer multiplication. No additional finite/overflow defect found.
- Separable blend has a full finite-difference coarse halo and same globally indexed lattice; outer normal/height equality and shared edges are appropriately tested over multiple seeds and nonbinary spacing.
- Clamp extension is not C1 at the reference boundary. The domain documentation explicitly discloses both this and potentially steep height-datum transitions; no claim of smooth inner derivative acceptance should be made.
- Regeneration digest covers float-submitted positions and normals deterministically. Color/material reconstruction is not hashed and should not be described as byte-identical full mesh/material reconstruction.
- Watershed pin set covers four 128m tiles spanning the complete reference patch. Diagnostic has no population/water scene; pinning is terrain retention only.
- Save/load and populated-world combinations reject via existing terrainStreamProbe option validation. No save schema mutation or source asset changes observed.

## Still required from coordinator

Native build/run receipt, actual before/join/after image inspection, updated verifier rejection probes, and final code review of coordinator follow-ups. Do not treat this provisional independent code review as visual acceptance.

## Follow-up review — coordinator revision, 2026-10-18

Static follow-up confirms cache now reads `mesh->vertex_positions`/`mesh->indices` and normals read back from `mesh->vertex_normals`. Evidence-strengthening item resolved. Final timed frame now renders the clamped endpoint before breaking; P3 endpoint false rejection resolved.

Verifier now enforces three diagnostic records, capture indices, finite scalar x intent, true Boolean visibility, and spatial brackets [24,96], [230,300], >=350. Independent fixtures under this review workspace `var/review-fixtures`: valid accepted; missing/null/string index, missing/null/string x, wrong location, missing/null/numeric/false visibility and missing diagnostics all rejected. One remaining malformed receipt acceptance was demonstrated: empty-array capture index (`capture: []`) is accepted because PowerShell array comparison returns a falsey empty list. Coordinator notified; scalar numeric index whitelist required before closing P2.

Independently viewed all three initial native captures in root `var/reports/runtime/20261018-033832-a0c74138`. Red proxy is visible over continuous engineering terrain; no open terrain crack apparent in those stills. Flat colors, black sky and limited slope cues do not establish final visual quality. Final rebuilt endpoint/cache revision native receipt remains pending.

### Verifier closure

Coordinator replaced coercion with scalar numeric whitelist (`int`, `long`, `double`, `decimal`) plus finite check for metrics, body x and capture index, and added exact final x=24 verification. Independent fixtures were updated only to final_body_x=24 to isolate malformed-field behavior against that new endpoint requirement. Valid baseline ACCEPT; all thirteen malformed cases REJECT, including the previously accepted empty-array index. P2 capture receipt-validation gap is closed for reviewed record fields. Empty image files still satisfy file existence; image contents require the explicit human/agent visual inspection performed separately.

All initially reported code defects/follow-ups are now resolved. Final native receipt for rebuilt endpoint/cache changes still pending.

## Final closure — 2026-10-18 03:42 America/Chicago

Canonical final evidence: root `var/reports/runtime/20261018-034050-13d0ec88`. Independently reran current `Verify-WatershedJoin.ps1 -Evidence <canonical>`: ACCEPT. Independently reran `Verify-TerrainStreaming.ps1` against coarse regression `20261018-034109-f688dd59`: ACCEPT. Read final receipt: 8 body tile crossings, 8 regenerated tiles, 0 reconstruction mismatches, 0 pinned watershed evictions, 8970 shared-edge samples, 907 interior samples with maximum error 1.7763568394002505e-15m, 8004 support samples, bottom-center error 3.814375133970316e-6m, rendered bounds center error 4.31432291065903e-5m, exact final x=24. Independently inspected final three PNGs: red marker visible in each, no apparent open crack in these views. Presentation remains flat engineering terrain with black sky; these captures provide no final-art acceptance.

**Final verdict: approve bounded watershed join diagnostic integration. No unresolved actionable finding in reviewed scope.** P2 capture validation and P3 endpoint timing findings are resolved; buffer evidence strengthening complete. Population traversal/activation/water integration, full-draft completeness and target 30 FPS remain unaccepted and outside this change.
