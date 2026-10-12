# Terrain surface implementation receipt

- Base: `078c9d6` (isolated branch `codex/terrain-surface-detail`).
- Wicked source: `df44c3db4c4927492bc9c791eac715d98d7ed091` (the pinned v0.72.120 revision).
- Implementation: one 512×512 sRGB base-color texture, 1,048,576 bytes, fixed five-octave periodic grain, seed-derived; terrain UVs use a 3 m repeat interval. Existing habitat vertex colors remain enabled and multiply the texture.
- Runtime helper reports `Applied`, `MissingMesh`, `GraphicsDeviceUnavailable`, or `TextureCreationFailed`, along with UV vertex count and texture dimensions/bytes.
- Validation used the installed Visual Studio 18 2026 / MSVC 19.51 toolchain. `TerrainSurfaceHeaderCheck` compiled the helper translation unit against the pinned Wicked headers. `TerrainTextureCheck` compiled and ran with exit code 0, confirming fixed byte count, identical bytes for a repeated seed, different bytes for a distinct seed, bounded red-channel values, and opaque alpha.
- Temporary CMake sources and outputs are under ignored `var/terrain-surface-header-check/`; they are not part of the change.
- Limit: the confirmed `wi::texturehelper::CreateTexture` path creates one mip level. Integrated runtime capture is still needed to check distant shimmer and player-facing detail; no visual-acceptance or frame-rate claim is made here.

## Source SHA-256

```text
stalthos-runtime/engine/terrain_surface.hpp af1d5dffc6d0cd250757cd56c3866ac1088b01d0f106b1f1285fdfdb72d01b49
stalthos-runtime/domain/terrain_texture.hpp 598c50f968cd31ad6f25aab70cdd1b378aefbb60d1c2cc90582f4c0fc0c97875
stalthos-runtime/docs/terrain-surface.md 08caf1b701198780bb02b9ba908dce32781de410f729cef7bd93168db7fdebf4
```

Root integration build and CTest 13/13 passed. Actual Wicked receipt var/reports/runtime/20261012-064940-5472f335/runtime-receipt.json confirms Applied, 512x512 texture, 1048576 bytes, 9409 UV vertices. Capture is diagnostic, with plain habitat composition and distant shimmer still unresolved.
