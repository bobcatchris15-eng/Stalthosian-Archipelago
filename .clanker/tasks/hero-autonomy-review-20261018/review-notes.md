# Hero autonomy working-draft review

Initial source findings: HeroSim/BodyPerception and NeedsCore suggestions exist, but native hero behaviour currently has no ordinary observation/goal/activity execution. The smallest useful loop is idle thirsty hero, body-observed creek target, ordinary forward movement, eligible existing contact once, and real thirst relief.

The connected spawn24,24 is approximately18m from the creek center near6.2,24, so a bounded20–24m body perception range and roughly9s travel can demonstrate a complete action without teleporting or new pathfinding. Existing creek physical eligibility is body-derived and shared registry contact already performs consumption then NeedsCore relief. Forage currently depends on imported plant presentation, making drink the smaller first activity.

Review blockers to falsify: direct W/S/A/D immediately wins before autonomous yaw/forward/contact; autonomous walking must not mark itself direct authority; camera-only movement does not grant resource observations; one activity completion must not mint repeated contact sessions; consumption and relief must use the same authoritative ledger/contact path as manual controls. Proxy anatomy, rough turning/pathing, small repertoire and visual polish are logged oddities after the intended activity works.
