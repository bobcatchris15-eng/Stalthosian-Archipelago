#pragma once

#include "environment_palette.hpp"
#include "deterministic_hash.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace stalthos::domain {

struct TerrainTextureMip {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t offset = 0;
    std::size_t byteCount = 0;
};

// Small seamless, neutral albedo detail for modulating terrain's habitat colors.
// Multi-scale organic noise (periodic gradient noise + domain warping) eliminates
// rectilinear grid artifacts. Mip chain (levels 0..9) removes minification shimmer.
struct TerrainTexture {
    static constexpr std::uint32_t kBaseSize = 512;
    static constexpr std::uint32_t kSize = kBaseSize;
    static constexpr std::uint32_t kMipLevels = 10;
    static constexpr std::size_t kBaseBytes = std::size_t(kBaseSize) * kBaseSize * 4;
    // Total footprint for all 10 mip levels:
    // 512^2 + 256^2 + 128^2 + 64^2 + 32^2 + 16^2 + 8^2 + 4^2 + 2^2 + 1^2 = 349,525 pixels * 4 bytes = 1,398,100 bytes (~1.33 MiB)
    static constexpr std::size_t kTotalMipBytes = 1398100;
    static constexpr std::size_t kMaxBytes = kTotalMipBytes;

    // Contiguous RGBA storage holding all mip levels in sequence.
    // Mip 0 (512x512) begins at offset 0, maintaining full backwards compatibility for data() pointers.
    std::vector<std::uint8_t> rgba;
    std::array<TerrainTextureMip, kMipLevels> mips{};

    const std::uint8_t* mipData(std::size_t level) const noexcept {
        return level < kMipLevels ? (rgba.data() + mips[level].offset) : nullptr;
    }
    std::uint32_t mipWidth(std::size_t level) const noexcept {
        return level < kMipLevels ? mips[level].width : 0;
    }
    std::uint32_t mipHeight(std::size_t level) const noexcept {
        return level < kMipLevels ? mips[level].height : 0;
    }
    std::uint32_t mipRowPitch(std::size_t level) const noexcept {
        return level < kMipLevels ? mips[level].width * 4 : 0;
    }
    std::size_t mipBytes(std::size_t level) const noexcept {
        return level < kMipLevels ? mips[level].byteCount : 0;
    }
};

namespace terrain_texture_detail {

// Quintic curve for C2 continuous derivatives (eliminates grid boundary kinks)
inline float quintic(float t) noexcept {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// 16 evenly spaced 2D unit gradients to avoid axis bias
inline void gradientVector(std::uint64_t hashVal, float& gx, float& gy) noexcept {
    static constexpr float kCos16[16] = {
        1.0f,  0.92387953f,  0.70710678f,  0.38268343f,
        0.0f, -0.38268343f, -0.70710678f, -0.92387953f,
       -1.0f, -0.92387953f, -0.70710678f, -0.38268343f,
        0.0f,  0.38268343f,  0.70710678f,  0.92387953f
    };
    static constexpr float kSin16[16] = {
        0.0f,  0.38268343f,  0.70710678f,  0.92387953f,
        1.0f,  0.92387953f,  0.70710678f,  0.38268343f,
        0.0f, -0.38268343f, -0.70710678f, -0.92387953f,
       -1.0f, -0.92387953f, -0.70710678f, -0.38268343f
    };
    const std::uint32_t idx = static_cast<std::uint32_t>(hashVal & 0x0F);
    gx = kCos16[idx];
    gy = kSin16[idx];
}

// Periodic 2D gradient noise with period 'cells'
inline float periodicGradNoise(std::uint64_t seed, float x, float y, int cells) noexcept {
    const auto wrap = [cells](int val) noexcept -> int {
        val %= cells;
        return val < 0 ? val + cells : val;
    };
    const float fx = (x * static_cast<float>(cells)) / static_cast<float>(TerrainTexture::kBaseSize);
    const float fy = (y * static_cast<float>(cells)) / static_cast<float>(TerrainTexture::kBaseSize);
    const int ix0 = static_cast<int>(std::floor(fx));
    const int iy0 = static_cast<int>(std::floor(fy));
    const float dx0 = fx - static_cast<float>(ix0);
    const float dy0 = fy - static_cast<float>(iy0);
    const float dx1 = dx0 - 1.0f;
    const float dy1 = dy0 - 1.0f;

    const int cx0 = wrap(ix0);
    const int cx1 = wrap(ix0 + 1);
    const int cy0 = wrap(iy0);
    const int cy1 = wrap(iy0 + 1);

    const auto getGrad = [seed](int cx, int cy, float& gx, float& gy) noexcept {
        const std::uint64_t coords = (static_cast<std::uint64_t>(cy) << 32U) | static_cast<std::uint32_t>(cx);
        const std::uint64_t h = hashMix(seed ^ hashMix(coords));
        gradientVector(h, gx, gy);
    };

    float gx00, gy00, gx10, gy10, gx01, gy01, gx11, gy11;
    getGrad(cx0, cy0, gx00, gy00);
    getGrad(cx1, cy0, gx10, gy10);
    getGrad(cx0, cy1, gx01, gy01);
    getGrad(cx1, cy1, gx11, gy11);

    const float dot00 = gx00 * dx0 + gy00 * dy0;
    const float dot10 = gx10 * dx1 + gy10 * dy0;
    const float dot01 = gx01 * dx0 + gy01 * dy1;
    const float dot11 = gx11 * dx1 + gy11 * dy1;

    const float u = quintic(dx0);
    const float v = quintic(dy0);

    const float nx0 = dot00 + u * (dot10 - dot00);
    const float nx1 = dot01 + u * (dot11 - dot01);
    // Gradient noise ranges approximately [-0.707, 0.707]; scale by sqrt(2) to approx [-1.0, 1.0]
    return (nx0 + v * (nx1 - nx0)) * 1.41421356f;
}

// Convert sRGB [0..255] to linear [0.0..1.0]
inline float srgbToLinear(std::uint8_t s) noexcept {
    const float c = static_cast<float>(s) / 255.0f;
    return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// Convert linear [0.0..1.0] to sRGB [0..255]
inline std::uint8_t linearToSrgb(float lin) noexcept {
    lin = std::clamp(lin, 0.0f, 1.0f);
    const float s = (lin <= 0.0031308f) ? (12.92f * lin) : (1.055f * std::pow(lin, 1.0f / 2.4f) - 0.055f);
    return static_cast<std::uint8_t>(std::clamp(std::round(s * 255.0f), 0.0f, 255.0f));
}

} // namespace terrain_texture_detail

inline TerrainTexture makeTerrainTexture(std::uint64_t seed) {
    TerrainTexture out;
    out.rgba.resize(TerrainTexture::kTotalMipBytes);

    // Compute mip offsets and metadata
    std::size_t runningOffset = 0;
    for (std::uint32_t level = 0; level < TerrainTexture::kMipLevels; ++level) {
        const std::uint32_t w = std::max(1U, TerrainTexture::kBaseSize >> level);
        const std::uint32_t h = std::max(1U, TerrainTexture::kBaseSize >> level);
        const std::size_t bytes = static_cast<std::size_t>(w) * h * 4;
        out.mips[level] = TerrainTextureMip{w, h, runningOffset, bytes};
        runningOffset += bytes;
    }

    // Seeds for multi-scale layers
    const std::uint64_t warpSeedX = hashMix(seed ^ UINT64_C(0x9E3779B97F4A7C15));
    const std::uint64_t warpSeedY = hashMix(seed ^ UINT64_C(0x517CC1B727220A95));
    const std::uint64_t macroSeed = hashMix(seed ^ UINT64_C(0xBF58476D1CE4E5B9));
    const std::uint64_t mesoSeed  = hashMix(seed ^ UINT64_C(0x94D049BB133111EB));
    const std::uint64_t microSeed = hashMix(seed ^ UINT64_C(0x1B87359329C7262D));
    const std::uint64_t speckSeed = hashMix(seed ^ UINT64_C(0xD1B54A32D192ED03));

    // Base Mip 0 Generation (512x512)
    for (std::uint32_t y = 0; y < TerrainTexture::kBaseSize; ++y) {
        const float fy = static_cast<float>(y);
        for (std::uint32_t x = 0; x < TerrainTexture::kBaseSize; ++x) {
            const float fx = static_cast<float>(x);

            // 1. Periodic low-frequency domain warp (cells = 4)
            // Displacement is strictly periodic across kBaseSize
            constexpr float kWarpAmp = 22.0f;
            const float warpX = kWarpAmp * terrain_texture_detail::periodicGradNoise(warpSeedX, fx, fy, 4);
            const float warpY = kWarpAmp * terrain_texture_detail::periodicGradNoise(warpSeedY, fx, fy, 4);

            // Warped coordinates wrapped seamlessly in [0, kBaseSize)
            const auto wrapCoord = [](float coord) noexcept {
                const float size = static_cast<float>(TerrainTexture::kBaseSize);
                coord = std::fmod(coord, size);
                return coord < 0.0f ? coord + size : coord;
            };
            const float wx = wrapCoord(fx + warpX);
            const float wy = wrapCoord(fy + warpY);

            // 2. Macro-scale sediment & loam mottling (cells = 4, 8)
            const float macro0 = terrain_texture_detail::periodicGradNoise(macroSeed, wx, wy, 4);
            const float macro1 = terrain_texture_detail::periodicGradNoise(hashMix(macroSeed + 1), wx, wy, 8);
            const float macro = macro0 * 0.65f + macro1 * 0.35f;

            // 3. Meso-scale clod, hummock and aggregate soil structure (cells = 16, 32)
            const float meso0 = terrain_texture_detail::periodicGradNoise(mesoSeed, wx, wy, 16);
            const float meso1 = terrain_texture_detail::periodicGradNoise(hashMix(mesoSeed + 1), wx, wy, 32);
            const float meso = meso0 * 0.60f + meso1 * 0.40f;

            // 4. Micro-scale pebble / mineral grit texture (cells = 64, 128)
            const float micro0 = terrain_texture_detail::periodicGradNoise(microSeed, fx, fy, 64);
            const float micro1 = terrain_texture_detail::periodicGradNoise(hashMix(microSeed + 1), fx, fy, 128);
            const float micro = micro0 * 0.55f + micro1 * 0.45f;

            // 5. Fine silica / mineral speckle (cells = 256)
            const float speckle = terrain_texture_detail::periodicGradNoise(speckSeed, fx, fy, 256);

            // Composite multi-scale organic albedo modulation:
            // Combines macro loam variation, organic meso clods, fine grit tooth, and mineral flecks.
            const float compositeGrain = (macro * 0.30f) + (meso * 0.35f) + (micro * 0.25f) + (speckle * 0.10f);

            // High neutral albedo (mean ~237) ensures underlying habitat vertex colors shine through.
            // Soil base tone modulates with subtle warm earthen bias (R > G > B).
            const float baseLuma = 237.0f + compositeGrain * 26.0f;
            // Chromatic nuance: warm loam patches have slightly richer red warmth,
            // mineral specks have crisp neutral highlights.
            const float warmth = macro * 2.5f + speckle * 1.0f;
            const float rVal = std::clamp(baseLuma + warmth + 1.5f, 205.0f, 255.0f);
            const float gVal = std::clamp(baseLuma + warmth * 0.4f - 1.5f, 200.0f, 255.0f);
            const float bVal = std::clamp(baseLuma - warmth * 0.8f - 6.0f, 195.0f, 255.0f);

            const std::size_t at = (static_cast<std::size_t>(y) * TerrainTexture::kBaseSize + x) * 4;
            out.rgba[at + 0] = static_cast<std::uint8_t>(rVal);
            out.rgba[at + 1] = static_cast<std::uint8_t>(gVal);
            out.rgba[at + 2] = static_cast<std::uint8_t>(bVal);
            out.rgba[at + 3] = 255;
        }
    }

    // Downsample Mip Chain (Levels 1..9) via sRGB-correct 2x2 box filter
    for (std::uint32_t level = 1; level < TerrainTexture::kMipLevels; ++level) {
        const auto& prevMip = out.mips[level - 1];
        const auto& currMip = out.mips[level];
        const std::uint8_t* prevSrc = out.rgba.data() + prevMip.offset;
        std::uint8_t* currDst = out.rgba.data() + currMip.offset;

        for (std::uint32_t y = 0; y < currMip.height; ++y) {
            for (std::uint32_t x = 0; x < currMip.width; ++x) {
                const std::uint32_t px0 = x * 2;
                const std::uint32_t px1 = std::min(px0 + 1, prevMip.width - 1);
                const std::uint32_t py0 = y * 2;
                const std::uint32_t py1 = std::min(py0 + 1, prevMip.height - 1);

                const std::size_t idx00 = (static_cast<std::size_t>(py0) * prevMip.width + px0) * 4;
                const std::size_t idx10 = (static_cast<std::size_t>(py0) * prevMip.width + px1) * 4;
                const std::size_t idx01 = (static_cast<std::size_t>(py1) * prevMip.width + px0) * 4;
                const std::size_t idx11 = (static_cast<std::size_t>(py1) * prevMip.width + px1) * 4;

                const std::size_t dstIdx = (static_cast<std::size_t>(y) * currMip.width + x) * 4;

                for (std::size_t ch = 0; ch < 3; ++ch) {
                    const float lin00 = terrain_texture_detail::srgbToLinear(prevSrc[idx00 + ch]);
                    const float lin10 = terrain_texture_detail::srgbToLinear(prevSrc[idx10 + ch]);
                    const float lin01 = terrain_texture_detail::srgbToLinear(prevSrc[idx01 + ch]);
                    const float lin11 = terrain_texture_detail::srgbToLinear(prevSrc[idx11 + ch]);
                    const float linAvg = (lin00 + lin10 + lin01 + lin11) * 0.25f;
                    currDst[dstIdx + ch] = terrain_texture_detail::linearToSrgb(linAvg);
                }
                currDst[dstIdx + 3] = 255; // Fully opaque
            }
        }
    }

    return out;
}

} // namespace stalthos::domain
