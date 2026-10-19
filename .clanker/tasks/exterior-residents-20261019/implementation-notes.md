# Connected exterior residents working draft

Connected preview adds four deterministic retained actors to the existing eight, in the same population/registry/scene. Original eight snapshots and IDs are copied unchanged into the extended habitat. Original nonconnected habitat, saves and counts are unchanged. Connected previews still refuse save/load.

`domain/exterior_residents.hpp` owns one tunable layout: corridor24/48/72/96,z24 and resident sites104,32 /116,18 /132,30 /144,20. All appended graph links are checked every0.5m against rendered triangle ground and maximum sampled grade0.6. The existing creek water mask applies only inside its finite original patch. Exterior validation rejects nonpositive ground: no exterior water mask, ocean/hydrology or freshwater resources are created or claimed. New nodes have no browse/water flags or anchors. Residents can eventually travel to the existing watershed resources along this graph.

`Run.ps1 -ExteriorResidentsProbe -Duration22` uses explicit hero startup104,24 and ordinary forward integration. NPCs initialize once at world creation; there are no NPC trial teleports. Existing awareness applies to the new residents. `-ConnectedWorld -Interactive` uses the normal watershed spawn and controls.

Limits: one coarse body-radius192m activation group, not independent region activation; narrow sparse graph/habitat with no local exterior resource affordance; provisional proxy presentation; no save-format work or full-world/performance acceptance. First native construction failure retained in20261019-031044-1261a8f6: finite creek-mask query outside patch, fixed by bounds guard.
