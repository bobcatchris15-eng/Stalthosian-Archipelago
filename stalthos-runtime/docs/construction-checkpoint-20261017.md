# Construction checkpoint — 2026-10-17

This packet continues the full first draft in Wicked Engine from `6da5298`.
Planning inputs remain read-only. This document records implementation and
diagnostic evidence; full draft, photoreal, foot-contact and 30 FPS acceptance
remain open.

## Jules backlog

The authenticated official CLI listed 31 newer sessions: 26 completed, four
awaiting feedback and one failed. Thirty sessions produced patches; the
movement session produced no diff and its fix was already represented locally.
The REST API key was absent; no credential store was inspected. Exact patches,
hashes, scope and overlaps are retained under ignored
`var/reports/jules-backlog-20261017/`. There were also 27 open GitHub PRs at audit
time. Remote session states and PR states were not modified.

Integrated bounded fixes:

- Restore the missing water-class scattering-albedo lookup; all-class
  scattering, extinction, depth transmission, turbidity and invalid-input tests
  replace legacy tests that expected the NameError.
- Normalize DOI locators without duplicating `10.` and recognize identifier
  prefixes regardless of case.
- Make the Windows non-ASCII Blender-banner regression independent of the
  machine's active locale; the production UTF-8 handling already existed.

Three candidates were rejected as written: a broken schema-test import, an
RFC3339 change accepting date-only input, and an entity/path packing optimization
that discards bits of the 64-bit entity ID. Twenty-four remaining patches have
explicit deferred dispositions, including overlapping native refactors and
performance changes that still need equivalence and benchmark evidence.

See `.clanker/tasks/jules-backlog-20261017/REVIEW.md` for per-session decisions.

## Smalltrotter source preparation

The exact mutual-nearest seam matcher now resolves 824 pairs while refusing
ties or insufficient first/second distance gaps. It requires one removed vertex
per pair, unchanged faces and bounds, one connected body, zero body boundary
edges, zero over-connected edges, and successful save/reopen validation.
Eyes and bristle fingerprints are preserved. Multires is removed from the
derived base cage only; this is not preservation of sculpt detail in that cage.

Independent review found that receipt-path aliases could overwrite the source
or derivative after hashes were checked. The fixed tool refuses such aliases
before Blender opens or a receipt writer runs, including existing hardlinks.
Eight plain Python path tests ran: seven passed, and symlink creation was
explicitly skipped for missing Windows privilege. Actual case aliases and
hardlinks passed. Nine optimized-Python seam checks also passed.

Root reran Blender 5.2 preparation into `var/seam-parent-20261017/`. Actual body
counts were 34,144 → 33,320 vertices, 33,318 unchanged faces, two → one
components, and 1,648 → zero boundary edges. Reopened topology and other meshes
were verified. Hashes:

- Source: `b987e7d7eac4dfe0e09e2e188a592236ed57eb5e00da7d8f7e6d1d901de15350`
- Derivative: `021a0e773fa13edfce4b0f32c00ef13b34281e7a94bb2b6ae57c3e92c774627b`

The source master remains immutable. The derivative is a coherent rough form;
materials, UVs, rigging and runtime replacement remain unfinished. The runtime
capture below still uses the older generated creature.

## Splitcrown terminal sprays

Staged conifer generation consumes explicit needle/scale anatomy. Small
broadleaf dimensions alone no longer trigger that route. Recognized stage
aliases use the same existing lookup semantics as the stage normalizer, and
needle dimensions must convert to finite positive units before Blender runs.
No-stage generation and non-tree routing retain their existing paths.

Fresh root generation, baking and GLB export succeeded for both stages. Each
export contains two materials with six verified embedded texture bindings.
Both exported GLBs exactly match the earlier isolated candidate hashes.

| Stage | Triangles | Imported trunk height | Whole height | Crown width |
|---|---:|---:|---:|---:|
| Sapling | 22,524 | 4.2122 m | 4.5650 m | 2.3851 m |
| Young adult | 15,948 | 14.0007 m | 14.9596 m | 6.2483 m |

Actual native run `var/reports/runtime/20261017-184447-ff7bdd35/` placed all
20 trees against rendered ground with zero fallbacks. The inspected gameplay
capture reads too sparse to accept canopy readability. The close view also
needs supporting twig/cluster review. Assets remain provisional; existing
runtime asset files were preserved. Geometry counts do not prove an FPS gate.

## Validation and continuation

The root Wicked Release build passed, and CTest passed 24/24 suites. The merged
plant-focused check passed 64 tests. Full merged toolkit validation and the
separate crossfade correction are recorded in the final task receipt:
`.clanker/tasks/continuation-20261017/receipt.json`.

The source graph was refreshed structurally: 6,296 nodes and 11,073 edges at
that refresh. The baseline was stale, so its larger delta spans older commits
as well as this packet. Graph evidence does not replace tests or actual assets.

Priority next work is source-derived Smalltrotter UV/material/rig preparation,
canopy supporting shoots and gameplay-distance coverage, and measured native
transition behavior. Gait foot locking, target-hardware frame time, four islands,
five playable lives and remaining full-draft breadth remain open.
