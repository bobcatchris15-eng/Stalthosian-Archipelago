# Terrain material refinement

## Summary

This bounded refinement addresses two observable defects in the initial terrain surface implementation:
1. **Repeated linear hatching artifacts**: The original 5-octave value noise sampled axis-aligned grid points with bilinear interpolation, causing vanishing derivatives along Cartesian axes and prominent rectilinear cross-hatch / banding patterns.
2. **Minification shimmer**: `wi::texturehelper::CreateTexture` previously created only a single mip level (`mip_levels = 1`). Repeated over a 192 m landscape at 3 m intervals (64 repeats across the ground plane), distant ground texels suffered severe sub-pixel aliasing and sparkling shimmer.

The refined implementation replaces value noise with **periodic 2D gradient noise** combined with **low-frequency domain warping**, eliminating axis-aligned grid saddles and linear hatching while preserving strict periodic boundary wrapping. It generates and uploads a complete **10-level mip chain** (512×512 down to 1×1) using sRGB-correct linear box downsampling, eliminating distant minification shimmer.

## Architecture and Contracts

### 1. Multi-Scale Organic Noise (`domain/terrain_texture.hpp`)
- **Base Dimensions**: 512 × 512 pixels (`kBaseSize = 512`).
- **Periodic Gradient Noise**: 16 unit gradient directions with Hermite quintic fade curves ($C^2$ continuous derivatives). Gradients wrap modulo octave cell count $C \in \{4, 8, 16, 32, 64, 128, 256\}$, ensuring strict $512 \times 512$ periodicity.
- **Low-Frequency Periodic Domain Warping**: A 4-cell periodic gradient displacement ($\pm 22\text{ px}$) gently warps the sample coordinates without breaking the seamless 512-pixel period. This removes rectilinear coordinate alignment, producing natural meandering sediment contours.
- **Layering**:
  - Macro sediment mottling (cells = 4, 8, weight 0.30)
  - Meso clod / aggregate soil structure (cells = 16, 32, weight 0.35)
  - Micro pebble / grit tooth (cells = 64, 128, weight 0.25)
  - Mineral / silica fleck speckle (cells = 256, weight 0.10)
- **High Neutral Ground Albedo**: Preserves underlying habitat vertex colors (`SetUseVertexColors(true)`). Mean albedo is maintained at $\sim 237$ within $[205, 255]$ with warm earthy ordering ($\text{R} \ge \text{G} \ge \text{B}$), avoiding flat grey or muddy darkening.

### 2. Complete Mip Chain and Memory Cap
- **Mip Levels**: 10 levels ($512, 256, 128, 64, 32, 16, 8, 4, 2, 1$).
- **Memory Footprint**:
  - Mip 0 (512×512×4): 1,048,576 bytes
  - Mip 1 (256×256×4): 262,144 bytes
  - Mip 2 (128×128×4): 65,536 bytes
  - Mip 3 (64×64×4): 16,384 bytes
  - Mip 4 (32×32×4): 4,096 bytes
  - Mip 5 (16×16×4): 1,024 bytes
  - Mip 6 (8×8×4): 256 bytes
  - Mip 7 (4×4×4): 64 bytes
  - Mip 8 (2×2×4): 16 bytes
  - Mip 9 (1×1×4): 4 bytes
  - **Total Contiguous Footprint**: Exactly **1,398,100 bytes** ($1.333\text{ MiB}$), strictly capped below $1.4\text{ MB}$.
- **Downsampling**: Offline sRGB-to-linear conversion, $2\times 2$ box filtering, and linear-to-sRGB mapping. Prevents gamma darkening across distance while keeping average luminance within 1.5% across all levels.

### 3. Wicked Engine Upload (`engine/terrain_surface.hpp`)
- Replaces `wi::texturehelper::CreateTexture` with direct `GraphicsDevice::CreateTexture` using:
  - `TextureDesc`: `width=512`, `height=512`, `mip_levels=10`, `format=Format::R8G8B8A8_UNORM_SRGB`, `bind_flags=BindFlag::SHADER_RESOURCE`.
  - `SubresourceData initData[10]`: each entry initialized with the respective mip pointer, row pitch (`width * 4`), and slice pitch (`width * height * 4`).
- Material continues to multiply vertex colors (`SetUseVertexColors(true)`), with roughness $0.96$ and metalness $0.0$.
- Reports `terrain_texture_bytes = 1398100` and `terrain_texture_width = 512`.

## Verification Evidence

1. **Domain Tests**:
   - `tests/terrain_texture.cpp`: Standalone portable test compiled with MSVC `/W4 /WX`.
     - Validates dimensions ($512 \times 512$, 10 levels, exact contiguous offsets).
     - Validates memory footprint ($1,398,100$ bytes total).
     - Validates bitwise determinism (same seed yields identical bytes; distinct seeds differ).
     - Validates channel bounds ($R, G, B \in [180, 255]$, $A = 255$).
     - Validates aggregate statistics (mean $R \in [225, 246]$, $R \ge G \ge B$, contrast $\Delta \ge 25$).
     - Validates seamless boundary wrap continuity across $x=511 \leftrightarrow x=0$ and $y=511 \leftrightarrow y=0$ ($\Delta \le 28$).
     - Validates mip luminance preservation across all 9 downsampled levels ($|\text{drift}| < 2.5$).
   - `ctest` on all 15 existing domain tests: 15/15 passed ($100\%$).
2. **Native Host Compilation**:
   - `Stalthos.exe` built in isolated worktree directory `var/build-host/Release/` against pinned Wicked Engine v0.72.120 (`df44c3db4c4927492bc9c791eac715d98d7ed091`) and release cache without errors.
3. **Native Host Execution**:
   - Executed native diagnostic run with `--inspection-camera --camera-distance 3 --duration 6`.
   - Result: `runtime-receipt.json` confirms:
     - `terrain_surface_applied: true`
     - `terrain_surface_status: 0` (Applied)
     - `terrain_texture_bytes: 1398100`
     - `terrain_texture_width: 512`
     - `terrain_uv_vertices: 9409`
     - `screenshot_saved: true`
   - Capture `draft-host.png` provides a static view of the new terrain material. Temporal shimmer reduction requires a matched motion capture; static imagery alone does not establish it.

## Limits and Pending Acceptance

- The implementation provides a complete mip chain and organic variation. Final material appearance and temporal aliasing acceptance remain pending matched runtime review.
- No frame-time performance or 30 FPS floor guarantee is claimed without dedicated benchmarking hardware.

## Parent integration evidence — 2026-10-12

Frozen9bf533c reviewed; parent Release build succeeded and16/16 native tests passed,
including the new portable terrain-texture tests. Actual Wicked run
`var/reports/runtime/20261012-232120-28557694` creates and renders the terrain
material: appliedtrue, width512, texturebytes1398100, UVvertices9409.
The static capture verifies the new material path. Matched motion evidence is
still required before claiming temporal shimmer reduction or final material acceptance.
