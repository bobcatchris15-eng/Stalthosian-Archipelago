# Diagnostic successor selection

The diagnostic Wicked host can continue a saved world after its current hero
dies. A save with recorded dead-hero truth and no current hero requires an
explicit `--successor-id <unsigned-id>` argument (PowerShell: `-SuccessorId`).
The ID must be greater than zero and absent from every existing live, retained,
and dead identity. The host adds that identity through the existing foundation
`setHero` path in the region attached to the greatest-ID dead-hero record in the
save (the ledger has no death timestamps, so this does not mean most recent by
time). It creates
a new Smalltrotter lifecycle at age zero and Hatchling stage; it does not restore
or revive the prior hero. The saved world seed, inventories, tree records, pose,
calendar clock, physical clock, and death records are restored before selection.

An omitted successor ID on a no-hero save, a successor ID on a live-hero save,
an ID of zero, and an ID already in the world all fail before saving. The native
failure path leaves an existing save untouched. `Run.ps1` validates the ID and
requires `-LoadState` before launching the host.

This is an explicit diagnostic identity-selection hook. It does not search the
world for an eligible living hatchling, model birth or eggs, or provide a
player-facing successor chooser. The host also has no age-specific hatchling
asset scaling, so the lifecycle record can say Hatchling while the diagnostic
creature presentation remains the existing adult-sized asset. Those population
and visual systems remain future work.

## Reproduction and evidence

The following was run against the pinned Wicked Engine Release cache with the
isolated worktree source. First, a fresh host was accelerated until Smalltrotter
reached terminal death and saved. A second host process loaded that save,
explicitly selected ID `2`, and saved the successor world:

```powershell
& .\stalthos-runtime\scripts\Run.ps1 -Duration 4.5 -CalendarScale 10000 `
  -SaveState .\var\saves\death.stl
& .\stalthos-runtime\scripts\Run.ps1 -Duration 0.8 -CalendarScale 0 `
  -LoadState .\var\saves\death.stl -SuccessorId 2 `
  -SaveState .\var\saves\successor.stl
```

The actual death receipt reports `lifecycle_stage: 4`, transitions through
`old` and `dead`, `hero_alive: false`, `hero_death_known: true`, and
`calendar_seconds: 34691.14695559256`. The successor receipt reports the same
loaded calendar value, loaded physical time `3.469114695559256`, successor ID
`2`, lifecycle age `0`, stage `0` (Hatchling), and preserved prior death truth.
The final successor save is read back by the host to check that the old dead
hero record remains serialized.

Recorded evidence from this run:

- Death: `var/reports/runtime/20261011-202804-96ab1f06/runtime-receipt.json`
- Successor: `var/reports/runtime/20261011-203211-5416e778/runtime-receipt.json`
- Rejections and unchanged prior-save hashes: `var/reports/successor-rejections.json`

The rejection probe used a prior-save SHA-256 of
`df4c86879c1d99e73756296227af8b66e033f9f0d4d55223ba380dbfd4b81322`. The
implicit successor, ID zero, dead ID `1`, and a successor switch from a live
hero save each exited with code `1`; every before/after SHA-256 matched.

The build and probes establish native host feasibility only. They do not
establish final visual quality, a population-level successor system, or the
project's frame-rate acceptance target.


Root merged Release proof: CTest 7/7; a new process restored the dead-world clocks, selected unused ID2, preserved ID1 death truth and wrote hatchling age 3.048189303779509. A subsequent process restored that exact age/stage and advanced to 4.954814708675258. Four rejected selections preserve the prior save hash. Raw local reports are `var/reports/packet-five-successor*.log` and `packet-five-root-rejections.json`.
