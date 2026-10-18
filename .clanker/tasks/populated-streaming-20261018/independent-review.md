# Independent adversarial review — populated streaming

Review time: 2026-10-18T09:38:46Z. Candidate: 0a03f44..796daa8. Root HALT absent. Reviewed implementation only; no implementation changes, builds, native hosts or dispatches run by reviewer.

```json
{
  "scope": {
    "graph_digest": "not refreshed; direct source review",
    "base_digest": "0a03f44",
    "artifacts": ["796daa8", "stalthos-runtime/scripts/Verify-PopulatedStreaming.ps1"]
  },
  "goal_alignment": {
    "verdict": "advances_partially",
    "drift": []
  },
  "findings": [],
  "unproven_claims": [
    "Native rendering, imported-asset traversal, exterior save/load and capture appearance require root native evidence; none available at reviewer checkpoint.",
    "No full-foot contact, regional population activation, final scenery or target-FPS acceptance established."
  ],
  "validation_debt": [
    "The proxy bottom metric is informational: original proxy geometry already has a 0.25m center-bottom gap, modified by terrain tilt. Verifier intentionally gates logical root support rather than foot/proxy contact.",
    "Terrain joins in position and shared vertex normals; material changes abruptly at the original patch because exterior material is untextured. This is replaceable draft presentation, not a geometric seam claim.",
    "Resident tile/mesh counts are bounded, while visited tile and digest metadata grows with unique explored tiles; synchronous generation remains documented debt."
  ],
  "asks": [],
  "effort": {
    "probes_run": 1,
    "attempted": "Read complete candidate diff and verifier; traced actual mesh topology through Wicked Entity_CreateMeshFromData and RenderedGroundSurface, eviction component ownership, ground sampler and support fallback paths, exterior load initialization ordering, world/body/source bounds, camera tether/residency, fauna height and identity paths, native sole and bone-proxy out-of-reference behavior. Independently enumerated clipping rectangles for all 121 tiles indexed -5..5 on both axes: 165 exterior patches, total area 1945600 matching 1408^2-192^2 exactly, zero interior overlap, zero invalid 2m spacings. No implementation defects found in this bounded review."
  }
}
```

Loaded-body residency is updated at main.cpp:739 before first initial-camera/body Ground query at main.cpp:807. Earlier Ground queries use only original local geometry/marker coordinates. The retained reference mesh has identity instance transform; exterior mesh/cache buffers share exact {a,b,c,b,d,c} topology validated by RenderedGroundSurface. Wicked creates mesh, material, transform and object on the same patch entity, so Entity_Remove releases their ECS ownership on eviction. Boundary normals explicitly copy original reference mesh normals; all remaining shared normals use the same source coordinates. Local mesh interior retains existing triangles, texture and vertex colors.

Body bounds ±12000 leave margin within source ±14000. The resident body neighborhood extends at least128m in each horizontal direction, comfortably exceeding the default camera orbit18m and tether. Camera queries outside original96m bounds therefore resolve through resident terrain. Native sole diagnostics reset outside reference bounds instead of retaining previous valid results; bone-proxy diagnostics use ReferenceLandscape inBounds and return invalid samples outside rather than inventing contact. Actual root support queries use resident triangles. Fauna remain simulated in their original habitat while away; pinned central tiles preserve their terrain. This packet does not implement regional activation.

The verifier checks required finite numeric types, nonnegative/integer counts, strict boolean claims, route thresholds, actor identity count/order/uniqueness, actual populated mode, capture brackets, visibility flags and capture file existence/size. It is a receipt acceptance check, not a cryptographic image/source proof. Root reports its independent valid fixture plus15malformed fixture cases pass/reject as intended; reviewer did not rerun those fixtures.

Disposition: source/probe review clear, conditional on root collecting actual populated traversal, exterior save/load and inspected native captures before integration/publication. A passing build alone is insufficient.

Additional malformed-input probe: **minor verifier finding**. Copied the root valid fixture into `C:/Users/Chris/AppData/Local/Temp/stalthos-stream-review-f113528c07b84db1b5c23bfa91caa544`, changed only `runtime-receipt.json` `resource_ledger_cap_exhaustions` from numeric0 to string `"0"`, then ran copied Verify-PopulatedStreaming.ps1 with -Evidence that directory. Exit0 accepted the malformed type. The numeric predicate is applied to terrain metrics but not this runtime metric; `-ne 0` coerces the string. Root notified; enforce Test-Number/integer/nonnegative for this runtime metric if all verifier inputs are intended fail-closed. This finding supersedes the empty findings array above until fixed; it does not establish any implementation/runtime failure. Total reviewer probes2.

Verifier finding closure: root added Test-Number validation for runtime resource_ledger_cap_exhaustions; independent rerun of the same string-zero fixture now exits 1 and rejects Actual populated host or shared resource ledger failed. Finding closed. Native runtime evidence remains pending. Total reviewer probes3.

## Native evidence addendum — independently inspected

Read `E:/Stalthosian Archipelago/var/reports/runtime/20261018-043923-84100b07/{populated-terrain.json,world-population.json,runtime-receipt.json}` and viewed all four populated-terrain-0..3.png captures. Native receipt proves actual populated mode and zero resource-ledger cap exhaustions;5779run calls match5779population frames/5779support samples. Eight actor IDs/species/alive truth retained in the same order; calendar age, needs and four actors travel distances advance.46240grounded updates equals8*(5779+1), including initial placement.

Traversal receipt: body24->544->24 at z24;8crossings/5visited tiles;29created,20evicted,8regenerated,0mismatches;13peak/9final residents;15006shared float-edge height/normal samples with zero differences. Root-height error3.8105063140392303e-6m; rendered object-center error3.0517578125e-5m; zero rendered-support fallbacks. Proxy bottom error0.27379546442534775m is explicitly informational, and the floating proxy is visible in captures.

All four captures show the brown marker and connected foreground terrain; no internal open crack observed at these views. The256m capture also clearly shows the finite resident terrain outer silhouette against black void at left. That is a visible draw-distance/residency cutoff, so the evidence supports internal join continuity, not seamless long-distance scenery. Very steep transition banks, abrupt material boundaries and sparse exterior remain visible engineering quality. Captures are stills, do not demonstrate locomotion anatomy/foot contact, and are no FPS/first-draft/final-art acceptance.

Review disposition after native traversal: no unresolved implementation defect; minor verifier malformed-type finding closed. Exterior save/load and focused regression results remain root verification responsibility; awaiting those paths if supplied. Native traversal source/receipt/capture review is complete.


## Final disposition after continuation evidence

Independently read traversal-save continuation log `var/reports/populated-streaming-traversal-continuation.log` and both populations in044118-666acb0e/044143-5d62918a. Direct compressed deep-JSON comparison of first actors_final and second actors_initial is exactly equal;8actors retained. Continuation log records resumed motion, zero water-restore error,2.5320287533325825e-11consumption error and zero ledger cap exhaustions.

Independently read exterior native terrain receipts044149-2ef43f99 and044223-d1cd9788: body remains x400,z24;8actors retained;13resident tiles; zero logical root error, bounds-center error, shared height/normal mismatch and rendered-support fallback. Reload runtime receipt explicitly state_loaded=true/state_saved=true, load_path exterior-resaved.stl, save_path exterior-resaved-second.stl and host_pose400,24. **Scope:** first exterior load starts from a deliberately controlled STL8 fixture, not a user-driven walk to400; first native run resaves it, and second native run reloads that native save. This establishes exterior initialization and native resave/reload feasibility at that coordinate, not a broad randomized save migration test. Reviewer did not independently decode the binary saved vertical pose.

Root reports nativeCTest30/30passed1.75s and toolkit2073passed/2skipped522.51s plus existing coarse/join regressions; reviewer did not rerun builds/tests or launch a competing host. No unresolved blocking/material/minor finding remains after verifier fix. The packet meaningfully advances the human world/system/population priority and is clear for reviewed integration within its stated scope. Regional activation, long-distance terrain presentation, creature foot contact and target30FPS remain unfinished. Final report complete; only this review document was changed in the review worktree.
