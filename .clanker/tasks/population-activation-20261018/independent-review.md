# Independent review: population activation candidate

Reviewed 2026-10-18. Scope: domain commit `1a9b696fa172676c863a4669ec73041c12b73af6` in isolated population-dormancy worktree, and coordinator root diff in fauna_scene.hpp, main.cpp, Verify-PopulatedStreaming.ps1 and Verify-PopulationContinuation.ps1. HALT absent at review entry. Read-only source review; no native run or build claimed. Only this review file written.

## Findings

1. **P1: zero scale hiding generates singular skinned armature matrices.** `stalthos-runtime/engine/fauna_scene.hpp:151` sets each replica root scale to `{0,0,0}`. Imported model armatures and bones descend from this root. Wicked Engine `var/vendor/WickedEngine/WickedEngine/wiScene.cpp:4021` unconditionally inverts each armature world transform and at line 4045 multiplies each bone world transform by this inverse. A zero scaled world matrix is singular, so the inverse/skinning path receives infinities/NaNs throughout dormancy. Pausing animation does not suppress RunArmatureUpdateSystem. Root update restores scale on reactivation, but that does not make dormant GPU bone data/bounds valid. Use renderability/visibility flags for descendant objects while retaining nonsingular transforms; preserve and restore their original flags. Native acceptance should check finite armature matrices/bounds while hidden and visible restoration after return, with the supplied imported mesh rather than proxy only.

2. **P2: streaming verifier does not verify its newly claimed population activation.** `Verify-PopulatedStreaming.ps1:36` now requires population_activation_implemented=true, but its population checks only retain eight identities. A host that continues to fully simulate fauna while away and emits that boolean still passes these assertions. The new main.cpp interval snapshots provide useful inputs but are not consumed here. Require a separate activation verifier or augment this one to assert outbound dormancy and inbound activation, positive dormant elapsed time, unchanged route/position/distance/injury/session history, expected elapsed needs/lifecycle, zero dormant resource consumption/events, and resumed active motion. This is a validation gap, not evidence that current domain progression is wrong.

## Domain/persistence observations

No new blocking domain or snapshot serialization defect found in the inspected candidate. advanceDormant validates times, computes on copies, preserves living route and pose, uses existing lifecycle/needs arithmetic, and commits only after all actors succeed. Death retains the final route pose and switches activity to Rest, which restore explicitly accepts for a corpse between sites. Snapshot JSON enumerates every current ActorSnapshot field plus population identity. Save/load reuses existing PopulationSnapshot and resource session persistence rather than inventing another representation. Body activation reads x/z directly and does not use camera position. Reentry closes interval before its first active update, preserving exact dormant endpoint evidence.

## Limitations, separate from defects

- Only the original watershed population participates. There is no regional spawn/activation population registry, hysteresis, or unloading of replica allocations.
- Dormant actor route and activity are intentionally retained while needs age; resting state remains the stored needs flag. Near activity transitions that flag can lag activity under the preexisting active update order. This behavior is inherited and explicitly documented as stored-state evolution, not newly introduced by advanceDormant.
- Needs at a death-crossing step follow existing active semantics; splitting elapsed time around death need not produce the same final corpse needs. Tests explicitly exclude death from split-step equivalence.
- Existing continuation verifier exercises a final active return for normal traversal, and requires resumed water consumption and movement. It does not by itself establish a save taken while dormant, loaded still dormant, and later returned. That case needs dedicated native evidence.
- camera_drives_simulation=false is a declared diagnostic field; the inspected activation branch is body-based, but no detached-camera perturbation comparison was run in this review.
- No runtime visibility, visual quality, frame time, full draft completion, or 30 FPS acceptance was performed by this reviewer.

## Recommendation

Domain candidate is suitable for coordinator integration and actual test verification. Resolve P1 before native activation acceptance. Resolve P2 with fail-closed activation evidence, plus a dormant-load/return probe. Retain this review as source-level evidence only.

## Follow-up source review: visibility fix and dormant continuation path

Reviewed current coordinator diff after the first review. HALT remains absent. **P1 is resolved at source level:** each replica now records original object renderability/shadow flags, disables those flags while dormant, and restores the originals on activation; it no longer changes root scale. The actual post-app.Run checks reject nonfinite or singular roots, nonfinite stored bone matrices, and incorrect object flags. This checks the supplied-skin path if native receipts contain positive skin_matrix_checks; positive hidden_object_checks and visible_object_checks are needed to demonstrate both states. Native images remain necessary to judge actual presentation.

The new --populated-departure path runs outbound and holds at x=544,z=24. Capturing traversalStartX from the loaded body pose lets a loaded-away traversal remain away until its return portion rather than immediately teleporting into the watershed at startup. The retained population snapshot is sampled before these diagnostic movements, so a dedicated verifier can compare exact saved/fresh-loaded actor state, then check dormant evolution and reactivation separately. No new source-level defect found in this follow-up.

P2 remains pending the separately assigned activation verifier. The source fix alone does not establish native save-away/load-away/return acceptance. No runtime run or build was performed by this reviewer.

## Follow-up: integrated activation verifier and actual native receipts

Reviewed verifier commit 547ce40 and its seven fixture tests. Ran `python -m unittest discover -s stalthos-runtime/scripts -p test_verify_population_activation.py -v` with bytecode disabled: **7/7 passed**. Ran the verifier using all four paths in `var/reports/population-activation-20261018/evidence.json` with **--require-skins: passed**.

Independently read real interval values. Departure has 127 active and 3117 dormant frames, 11.558056607376784 dormant physical/calendar seconds; fully dormant reload has zero active and 903 dormant frames, 3.6239448026026366 seconds; returned run has 579 active and 2710 dormant frames, 10.188538004891598 dormant seconds. All eight actors in each interval increased age by the reported elapsed calendar seconds and hunger/thirst by .0001/.0002 times elapsed seconds. The verifier compares every retained route/pose/injury field, checks no dormant resource consumption or events, and compares complete snapshots across both save/load boundaries. Skin matrix checks are positive in all runs (1,946,400; 541,800; 1,973,400; 966,600). Away hidden checks and restored visible checks are positive. Observed transition radii are 202.944974m when exiting and 191.897237m when entering, consistent with 192m activation. Camera run has 1611 active frames, zero dormant frames and no activation transitions.

**P2 is substantially addressed for the actual supplied evidence**, but two fail-closed validation gaps remain in this verifier version:

- number(...physical_seconds/calendar_seconds) accepts zero. Positive dormant frame counts with unchanged snapshots and zero elapsed interval totals could still pass elapsed-dormancy assertions. Require positive physical and calendar elapsed values for these deliberately timed probes and add zero-duration malformed fixture coverage.
- Transition body x/z are checked only for finite type, never against radius and physical_active. Require physical_active to equal hypot(body_x,body_z)<=activation_radius_m and add opposite-side malformed transition coverage.

Both gaps were reported to coordinator. Actual native evidence is positive and correctly located; these findings concern malformed-proof rejection, not a defect observed in this native run. P2 can close after those two assertions and negative cases pass. No native execution or visual capture inspection was performed by this reviewer; existing receipts were independently checked.

## Closure review

Rechecked the updated verifier: check_interval now requires positive physical and calendar elapsed time; check_transition compares activation against the actual body radius. Fixtures now test zero elapsed plus both valid/opposite-side transition claims. Reran fixtures: **9/9 pass**. Reran the actual four-run manifest with --require-skins: **passed**. **P2 closed.** Together with the source visibility fix and positive native post-engine matrix/visibility checks, **P1 is also closed for this bounded activation checkpoint**. No unresolved finding remains from this review.

Inspected coordinator's audit-save-ledger.py and independently reran its read-only parsing/assertion portion against the three actual STL8 files without rewriting its JSON output. Assertions passed: departure/dormant resource quantities and complete ledger session tuples compare exactly; all eight actor/session mappings match through return; all four prior history keys remain present with nondecreasing consumed amounts; departure/dormant body x=544 and return x=24. The receipt uses the phrase byte_values_exact, but the audit compares decoded integer/double values rather than complete save bytes, which intentionally differ in clocks/needs. No broader bitwise file identity claim is made.

No image inspection was added by this reviewer. The coordinator's framed population capture uses a controlled copy with changed body pose, so it can illustrate restored actor presentation but cannot substitute for the actual native departure/load/return proof. Native receipts and exact snapshots provide that continuity proof. Regional populations, physical unloading, death-step split equivalence, final visuals, full draft completion and 30 FPS acceptance remain outside this bounded result.
