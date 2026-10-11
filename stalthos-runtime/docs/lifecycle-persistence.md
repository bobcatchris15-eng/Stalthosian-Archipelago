# Hero lifecycle persistence

The persistence DTO can carry an optional `HeroLifecycleSnapshot` for the current hero. It binds the lifecycle record to the active hero identity and stores the fictional species profile, calendar age in the lifecycle's existing units, and the corresponding stage. The host can attach a snapshot with `captureHeroLifecycle(lifecycle, heroId)` and reconstruct it with `restoreHeroLifecycle(save, legacySpecies)`. The existing `captureSave` and `restoreSave` signatures remain unchanged so callers can adopt this seam independently.

## STL3 wire record

All new writes use the `STL3` magic and version 3. The STL2 fields retain their order. Immediately after the optional host pose, STL3 stores a one-byte lifecycle presence flag. A value of zero means no snapshot. A value of one is followed by exactly 18 bytes: hero ID (`u64`), species (`u8`), calendar age (IEEE 754 `f64`), and stage (`u8`). Integers are little-endian. The exact encoded-size calculation includes the presence byte even when absent and the 18-byte payload when present; the existing 64 MiB file bound applies before a previous save is replaced.

STL3 reads reject noncanonical presence flags, invalid species or stage values, truncated fields, trailing bytes, and unsupported versions. A snapshot must refer to the current hero's exact ID, and its species must agree with any retained record for that hero. Its age must be finite and nonnegative, and its stage must match the selected species profile's lifecycle thresholds. The current hero is live, so a `Dead` snapshot is invalid. The host records death through the existing world death state and omits lifecycle data after the current hero is removed.

## Legacy migration

Readers continue to accept the existing STL1 and STL2 layouts. Those versions contain no lifecycle evidence, so reading them leaves `heroLifecycle` empty. They are not assigned an invented species or age. When a caller needs a lifecycle object for a legacy save, it supplies a provisional species to `restoreHeroLifecycle`; a matching retained hero record supplies the known species in preference to that fallback. The resulting lifecycle starts at age zero because no legacy age evidence exists. Saving the migrated DTO writes STL3, with lifecycle absence preserved until the host attaches a snapshot.

STL1's pre-existing defaults and revision reconstruction remain intact. STL2 keeps its physical time, pose, and stored region revisions. No other legacy fields are inferred by the lifecycle migration.

## Current draft boundary

This seam persists age and life stage only. It does not complete gameplay needs, social bonds, reproduction, parenting, or dependent-care behavior; those remain unfinished parts of the full game draft. A lifecycle save roundtrip proves persistence behavior, not complete lifecycle gameplay or game acceptance.
