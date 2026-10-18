# Current world construction checkpoint — 2026-10-18

Priority: world, simulations, systems and populations. Replaceable assets best effort; HeroViewer/Smalltrotter polish off critical path.

## Validated continuation

Immutable world::WatershedTerrain now preserves exact ReferenceLandscape heights throughout the original192m square with unchanged local/world coordinates and Y. It blends through a bounded outer band to unchanged Archipelago terrain; all four outer256m boundaries agree exactly in height and numerical normals. Source interior noise remains discontinuous and the blend can be steep; no inner C1 claim.

The new optional --watershed-join-probe uses128m tiles with64 cells at2m spacing and pins four central tiles. Final18s native outbound/return24->544->24 crossed8 body tile boundaries, visited5 body tiles, created29/evicted20/regenerated8, peaked at13 residents, evicted zero pinned watershed tiles. All8970 shared float edge positions/normals agree exactly; regenerated position/normal digests match. All907 interior triangle samples match original2m rendered ground within1.8e-15m. Actual engine mesh buffers back the contact cache. Proxy bottom-center error<3.82e-6m; post-update bounds center error<4.32e-5m. All3 captures inspected:405 red marker pixels each and positive visibility, no apparent open crack. Coarse colors/black sky/accelerated proxy with occlusion disabled are engineering evidence only.

Release built; native CTest30/30 passed(1.91s); toolkit2073 passed/2 skipped(522.34s). Existing coarse streaming regression passed. Population save/load retained8 exact actor states with resumed motion; creek restoration/consumption numerical errors<1.2e-10 and zero ledger cap exhaustion. Verifier rejects13 malformed receipts, including empty-array capture index. Independent review complete, no unresolved finding.

Receipt: `.clanker/tasks/watershed-join-20261018/receipt.json`; canonical native evidence: `var/reports/runtime/20261018-034050-13d0ec88`. Earlier terrain and fauna receipts remain historical evidence.

## Next concrete work

Adopt the joined terrain in the actual populated-world host. The normal populated-world mode STILL uses its original192m detailed patch; diagnostic joining does not change gameplay or saved positions. Extract reusable streamer/surface queries, avoid central mesh overlap, preserve water/resources and original2m triangle contact, address FaunaScene single-mesh ground lookup and ReferenceLandscape-only camera bounds. Then perform actual populated outbound/return traversal and regional habitat placement, followed by body-driven activation/retention and coarse dormant evolution. Brief: `.clanker/tasks/watershed-join-20261018/next-task.md`.

Inactive saved actors remain frozen. Resource history still capped at256 actor/resource keys; production epochs/crowding remain open. No full-draft, final art, full-foot contact or target30FPS acceptance.

## Collection and preservation

No outstanding world tasks at entry. Isolated domain implementation collected; independent review and one follow-up collected. Three dispatches this wake; no AGY/Jules dispatch or old patch retrieval. Prior Jules inventory remains26 completed/4 awaiting feedback/1 failed with30 patches inventoried; do not refetch. No active worker remains. All84 preexisting untracked files verified unchanged; worktrees/history/open apps/read-only planning/immutable assets preserved. Check HALT before work/dispatch. Authorized reviewed publication is origin main only; no force push/releases/deployment/paidAPI/credential changes.
