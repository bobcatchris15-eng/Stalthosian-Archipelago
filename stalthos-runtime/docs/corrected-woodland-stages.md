# Corrected Splitcrown stages in Wicked — 2026-10-13
Parent verification loaded both isolated baked stage exports into the actual native host. Capture and raw receipt: var/reports/runtime/20261013-180612-4026d0fd. Parent material/hash/placement audit: var/reports/packet-twentytwo-stage-parent-audit.json.

| Stage | GLB SHA256 | Structure height | Whole height | Instances |
|---|---|---:|---:|---:|
| sapling | be76b1f85bc1960e0e58ffb96e87e0419ec90d42b0c0dd05b189fe58ba7eadb5 | 4.212208 m | 6.547903 m | 6 |
| young_adult | 8117f868c32091b24619775287b3f7954395628bb879081f63983105260c3a43 | 14.000714 m | 16.239569 m | 14 |

Each GLB contains two materials with embedded base-color and normal textures. Existing export_baked validation reports two materials and six checked texture bindings, with no issues. Parent independent audit verifies actual binding references, embedded images, hashes and native geometry/placement. Assets remain under separate ignored var/worktrees/agy-splitcrown-{sapling,young_adult}-baked-stage/var/stage-bake/exports directories. Original corrected source workspaces remain untouched.

The host placed all twenty proposals on rendered ground triangles with zero ground fallbacks and zero skipped instances. This replaces the prior missing-sapling/mature-sized prototype experiment. The inspected twelve-meter inspection-camera capture shows colored young trees and saplings beside the creek.

Limits: young-adult crown width measures 14.101810 m against an authored 6.5 m target. Foliage is coarse and crowns extend beyond this framing. The terrain and ground cover remain visibly unfinished. Stage suitability is provisional; structure-height agreement does not establish correct crown anatomy. Degraded LOD quality remains unmeasured. No animation, finished environment, full draft or 30 FPS acceptance follows from this capture. A diagnostics worker is removing stale hardcoded source-fix/missing-sapling descriptions, and an isolated morphology worker is correcting crown proportions before another bake/native comparison.

Young-adult worker froze documentation and receipt at 16f7517; its unchanged source task produced assets using existing commands. Sapling worker produced usable baked exports but no committed evidence document and ended after waiting on background work. Parent verified its output independently; worker SUCCESS alone was not accepted. Existing toolkit source is unchanged since the 1,997-pass/two-skip suite.
