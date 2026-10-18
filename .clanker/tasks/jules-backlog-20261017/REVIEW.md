# Jules backlog audit â€” 2026-10-17

Base: `6da529846b741d7d035549a05986208cb9b23059`. 31 newest sessions through 3112312361709896141; older sessions deliberately not repulled.

Source: authenticated official Jules CLI `remote list --session` and `remote pull --session ID`, without --apply. JULES_API_KEY was absent; no credentials read or printed. No remote jobs launched, no remote states changed. Patches stored as exact process stdout bytes with SHA256 in ignored report inventory.

## Counts

{
  "sessions": 31,
  "remote_states": {
    "Completed": 26,
    "Awaiting User Feedback": 4,
    "Failed": 1
  },
  "patches": 30,
  "empty_pulls": 1,
  "dispositions": {
    "defer-blender-refactor": 3,
    "defer-native-host-refactor": 1,
    "validated-integrated-by-root": 2,
    "defer-test-refactor": 1,
    "reject-as-is": 3,
    "defer-equivalence-benchmark": 2,
    "defer-native-test-harness": 1,
    "defer-test-only": 11,
    "defer-combine-test-file": 1,
    "defer-behavior-change": 1,
    "defer-low-value": 1,
    "defer-overlapping-refactor": 2,
    "no-diff-already-addressed": 1,
    "validated-isolated": 1
  }
}

All 30 actual patches passed git apply --check individually against the isolated baseline. This does not establish mutual compatibility or correctness. No exact full-patch duplicates; files overlap as recorded in inventory.

## Validated changes

- b3ac6ac: DOI and identifier fix; 324 formats/schema tests passed in 38.73s. Baseline https://doi.org/10.1234/example normalized incorrectly to 10.10.1234/example; candidate gives 10.1234/example.
- 002f11d: missing SCATTERING_ALBEDO lookup; 109 optics tests passed in 0.29s. Baseline NameError reproduced; candidate scattering_coefficient_m(535)=0.03894153846153846. Tests cover all classes, a+b extinction, reference albedo, depth transmittance, turbidity and unknown inputs.
- Combined optics + Blender suite: 175 passed in 174.03s (0:02:54), exit 0.
- fdcbd83: host-independent Windows banner regression; 3 version tests passed/63 deselected in 0.23s. Isolated only at root suite freeze; production UTF8 runner already existed.

Root notified first two candidates and confirmed integration. Root owns combined full-suite validation and source publication. No FPS, visual, new-world or Blender production acceptance established here.

## Per-session disposition

| Session | Remote state | Disposition | Review |
|---|---|---|---|
| 16510317542034409616 | Completed | defer-blender-refactor | Large creature build_body extraction. Needs Blender generation output/rig anchor comparison; no player-visible improvement established. |
| 13746440482367683135 | Completed | defer-blender-refactor | Extracts neutral idle authoring helpers. Needs generated clip/foot-contact comparison and Blender validation. |
| 18389713807093418238 | Awaiting User Feedback | defer-native-host-refactor | Extracts main_config.hpp parser and adds domain engine-main test; CMake/main.cpp overlap with 1174516393090302710 and host work. Needs native build plus real argument compatibility. |
| 4330081873842231404 | Completed | validated-integrated-by-root | 002f11d; restore missing albedo lookup; 109 optics tests pass; all-class physical API invariants; baseline NameError reproduced. |
| 3785714033331623415 | Completed | defer-test-refactor | Splits existing camera persistence test; verify all prior assertions retained. Low urgency; no new production behavior. |
| 5600814607861772839 | Completed | defer-blender-refactor | Bake pass extraction. Needs actual multi-material bake comparison, material completeness/warnings and outputs; overlapping active bake work possible. |
| 8520825439583794197 | Awaiting User Feedback | reject-as-is | Packs 64-bit Wicked Entity and uint8 path into uint64 with Entity<<8; truncates top 8 bits and can alias identities. wiECS.h confirms Entity=uint64_t. Includes 3 one-off patch scripts and unrelated terrain-test changes. |
| 12940647787556515601 | Completed | defer-equivalence-benchmark | Floodplain dilation becomes multisource BFS; likely same Chebyshev radius, but needs randomized mask-equivalence and actual performance evidence; broad incidental formatting. |
| 1174516393090302710 | Completed | defer-native-test-harness | Adds fake WickedEngine header/environment scene tests; adds CMake, ignore, PR-body artifacts. Tests cannot replace real Wicked runtime evidence; strip incidental files and validate native isolation. |
| 16822662024650619227 | Completed | defer-test-only | Hydrology coverage in new tests/environment package; review assumptions, run isolated and full suite before promotion. |
| 9000293867114738443 | Completed | defer-test-only | Biome summary coverage; overlaps other biome test scope but separate paths. Run focused tests before promotion. |
| 13305320810050235026 | Completed | defer-test-only | JSON status stdout/newline contract coverage. Independent of sha256 test patch, same core test file. |
| 3833971034897626593 | Failed | reject-as-is | Remote Failed. Test imports nonexistent schemas.registry.DefinitionError, implementation defines different class in loader and raises UsageError; message expectation differs too. |
| 6071172401981107971 | Completed | defer-test-only | Malformed contract JSON test; clears registry cache before call but should restore isolation. Shares test_schemas append context with multiple peers. |
| 1176426733573799088 | Completed | defer-test-only | Definition file listing/filter coverage; overlapping test_schemas append context. |
| 11726424835917292372 | Completed | defer-combine-test-file | Adds test_formats.py media-type tests; add/add conflict with accepted DOI tests. Merge useful cases, do not replace accepted file. |
| 17193358324930287862 | Completed | defer-test-only | Degraded policy option/environment precedence coverage. Verify existing context fixture API before promotion. |
| 16699706203465748385 | Completed | validated-integrated-by-root | b3ac6ac; fix duplicate DOI prefix and identifier case; 324 formats/schema tests pass. Test style cleanup only. |
| 3621563751622146992 | Awaiting User Feedback | defer-test-only | Compression byte-accounting/degraded flag coverage; verify actual report API and expected zero-byte ratio. |
| 16124953348826853924 | Completed | defer-test-only | Heightfield subgrid/synthesis equality coverage; verify constructor/current positional API. |
| 584583452440779116 | Completed | defer-behavior-change | Changes biome classification priority to make rainforest reachable; test-first proposal needs planning/world-layout review before changing generated content. Root managing environment work. |
| 4680136894348644119 | Completed | defer-test-only | sha256 known-answer tests; same core test file, separate section. Strip trailing whitespace. |
| 1286278668489919798 | Completed | defer-test-only | Schema validation issue formatting/limit coverage; overlapping test_schemas append context. |
| 6182029514532585618 | Completed | reject-as-is | RFC3339 replacement accepts date-only via datetime.fromisoformat; broadens contract unexpectedly. Tests explicitly declare date-only okay. Preserve current contract pending deliberate stricter design. |
| 3091204301234114151 | Completed | defer-test-only | JSON pointer token escaping coverage; overlapping test_schemas imports/context. |
| 8334866519898718171 | Completed | defer-low-value | string_view at scene initialization, not per-frame; remote microbench claims unverified. Includes generated PR description. No target FPS claim justified. |
| 11764460197354008542 | Completed | defer-equivalence-benchmark | Direct data indexing in slope calculation removes get overhead; test exact numeric/cache behavior and benchmark representative grids before promotion. |
| 2232192937130928746 | Completed | defer-overlapping-refactor | Hash helper extraction overlaps 6025035057603222752 across foundation/palette/habitat/reference_landscape. Choose one, verify deterministic outputs byte-for-byte and full native suite. |
| 6025035057603222752 | Completed | defer-overlapping-refactor | Math/hash helper extraction overlaps 2232192937130928746. Do not blindly stack. Requires deterministic output comparison and native tests. |
| 8271069973957152765 | Awaiting User Feedback | no-diff-already-addressed | No diff found in remote VM. Existing hero_view_controller.hpp and tests retain attenuated outward / full inward/lateral motion. No new patch to integrate. |
| 3112312361709896141 | Completed | validated-isolated | fdcbd83; 3 Blender version tests pass. Host-independent cp1252 regression only; production runner UTF8 fix already present. Root full suite frozen; defer integration. |

## Next bounded work

1. Finish root full-toolkit/native/build checks and inspect runtime evidence of current asset integration.
2. On the next toolkit checkpoint, integrate fdcbd83 regression and combine media-type tests into accepted test_formats.py after focused validation.
3. Validate floodplain BFS and slope direct indexing through randomized exact equivalence and representative microbenchmarks; retain only proven performance improvements.
4. Review reachable-rainforest classification against authored ecological rules; a branch ordering change changes world output and is not merely test cleanup.
5. Treat the two overlapping native hash/math extractions as alternatives and retain byte-identical deterministic fixture outputs before accepting either.
6. Defer large creature/rig/bake refactors until current player-facing asset seam, surface and movement work has real Blender/runtime evidence.

Evidence paths: var/reports/jules-backlog-20261017/inventory.json, sessions.txt, <session>.patch, <session>.stderr.txt, baseline-probe.txt, candidate-probe.txt. Collection and classification scripts live beside this receipt.


## Root integration update — 2026-10-17

The third validated candidate fdcbd83 (UTF8 test) is now integrated in ae6f5ce.
Final root toolkit suite:2073 passed,2 skipped in627.89s. Thus3 validated candidates
are integrated;24 deferred,3 rejected-as-written and1 no-diff remain the audit disposition.
GitHub read-only inventory found27 open PRs. Remote session/PR states are unchanged.
New world environment session3306157609621882461 is separate from this old backlog.
