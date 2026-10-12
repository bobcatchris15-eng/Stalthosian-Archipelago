# Hero needs persistence

The persistence DTO can carry an optional `HeroNeedsSnapshot` for the current hero. It binds the needs record to the active hero identity and stores the fictional species profile, hunger, thirst, fatigue, injury, and resting flag. The host can attach a snapshot with `captureHeroNeeds(needsCore, heroId)` and reconstruct it with `restoreHeroNeeds(save, legacySpecies)`.

## STL4 wire record

All new writes use the `STL4` magic and version 4. Immediately after the optional hero lifecycle record (its presence byte and, when present, its payload), STL4 stores a one-byte needs presence flag. A value of zero means no snapshot. A value of one is followed by exactly 42 bytes: hero ID (`u64`), species (`u8`), hunger (IEEE 754 `f64`), thirst (IEEE 754 `f64`), fatigue (IEEE 754 `f64`), injury (IEEE 754 `f64`), and a resting flag (`u8` where 1 is resting and 0 is not). Integers are little-endian. Needs and lifecycle records are independently optional.

STL4 reads reject noncanonical presence flags, invalid species, normalized need values outside `[0.0, 1.0]`, NaN values, noncanonical resting flags, truncated fields, trailing bytes, and unsupported versions. A needs snapshot must refer to the current hero's exact ID, its species must agree with any retained record for that hero, and its species must match the lifecycle species if both are present. The current hero is live, so if it dies, needs data should be omitted by the host.

## Legacy migration

Readers continue to accept the existing STL1, STL2, and STL3 layouts. Those versions contain no needs evidence, so reading them leaves `heroNeeds` empty. When a caller needs a `NeedsCore` object for a legacy save, it supplies a provisional species to `restoreHeroNeeds`; a matching retained hero record or lifecycle record supplies the known species in preference to that fallback. The resulting needs core starts with zeroes for all needs (optimal state) because no legacy needs evidence exists. Saving the migrated DTO writes STL4, with needs absence preserved until the host attaches a snapshot.

## Current draft boundary

This seam persists only current hero needs. It does not complete host integration (feeding, navigation, autonomous goals), finding resources, ecological budgets, or controller authority changes. It does not provide persistence for herd members. A needs save roundtrip proves persistence behavior, not complete needs gameplay or game acceptance.

## Testing

The standalone persistence checks use an explicit `check` helper rather than `assert`, so they remain active in optimized Release builds with `NDEBUG`. From `stalthos-runtime`, compile and run them with Visual Studio's x64 C++ environment:

```
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /std:c++17 /W4 /WX /EHsc /permissive- /O2 /DNDEBUG /Fe:persistence-release.exe tests\persistence.cpp
persistence-release.exe
```

Verified in optimized Release mode with `/O2 /DNDEBUG`: `11/11 tests passed` using the Visual Studio 2026 x64 C++ compiler. The run was offline and did not build or launch Wicked.

The standalone run covers existing persistence, foundation, lifecycle, and needs scenarios, all four independent lifecycle/needs presence combinations, STL1/STL2/STL3 legacy needs baselines, invalid identities/species/values/flags, truncated and trailing payloads, exact encoded sizes, and preservation of a prior save after rejected writes. It runs offline without Wicked or network.

No host or player save integration is included. The runtime host must still attach `captureHeroNeeds` output to the `SaveData` DTO, pass the restored DTO through `restoreHeroNeeds`, and decide how a newly selected hero receives its needs state. This module does not select that hero or decide terminal-death policy.
