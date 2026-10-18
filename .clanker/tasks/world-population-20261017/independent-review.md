# Independent review — populated fauna integration

## Result

No actionable blocker found in the reviewed integration diff.

The population advances from `TimeStep` whenever `--populated-world` is enabled, without checking camera mode, camera focus, or camera input. The detached-camera and orbit code updates camera state separately. Fauna positions are projected onto the cached rendered terrain mesh by `FaunaScene::update`.

Hero animation handling preserves the pre-fauna animation count. The hero lifecycle, playback, and pose-measurement loops operate only on indices below that count; fauna clones are appended and updated through their own replicas.

Resource continuity is connected to the shared ledger: load restores it before fauna setup, fauna allocates sessions above the maximum restored session ID, each tick refreshes site availability before simulation and applies cumulative contacts after it, and save captures the ledger. Existing evidence in `var/reports/world-population-20261017/runtime-audit.json` reports exact water continuation (`water_continuation_abs_error: 0`, `water_continuation_pass: true`). The asset-backed 30-second run reports eight actors, all four activities, food consumption `0.7787083228910324`, and water consumption `0.5191388819273549`; the 4-second proxy load records the saved world resource state and continued water consumption.

## Clear limitations

- Fauna identities, positions, ages, needs, and activities are not persisted. The population is reconstructed on load; the shared resource ledger is what continues across the save.
- Species use best-effort shared creature/proxy presentation. The report marks meshes as best effort and frame time as unmeasured, so this run does not establish species-specific visual fidelity or performance acceptance.
- The save/replay audit validates resource continuation, not persistence of individual fauna biographies.
