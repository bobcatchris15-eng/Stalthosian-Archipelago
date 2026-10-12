#pragma once

#include "environment_palette.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace stalthos::domain {

// Small seamless, neutral albedo detail for modulating terrain's habitat colors.
// Fixed dimensions and octave count keep generation bounded and reproducible.
struct TerrainTexture {
    static constexpr std::uint32_t kSize = 512;
    static constexpr std::size_t kMaxBytes = std::size_t(kSize) * kSize * 4;
    std::vector<std::uint8_t> rgba;
};

namespace terrain_texture_detail {
inline float lattice(std::uint64_t seed, int x, int y, int period) noexcept {
    const auto wrap = [period](int value) { value %= period; return value < 0 ? value + period : value; };
    const std::uint64_t coords = (std::uint64_t(wrap(y)) << 32U) | std::uint32_t(wrap(x));
    return static_cast<float>(hashUnit(hashMix(seed ^ hashMix(coords))));
}

inline float smooth(float t) noexcept { return t * t * (3.0f - 2.0f * t); }

inline float octave(std::uint64_t seed, std::uint32_t x, std::uint32_t y, int cells) noexcept {
    const float fx = (static_cast<float>(x) * cells) / TerrainTexture::kSize;
    const float fy = (static_cast<float>(y) * cells) / TerrainTexture::kSize;
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
    const float tx = smooth(fx - ix), ty = smooth(fy - iy);
    const float a = lattice(seed, ix, iy, cells), b = lattice(seed, ix + 1, iy, cells);
    const float c = lattice(seed, ix, iy + 1, cells), d = lattice(seed, ix + 1, iy + 1, cells);
    const float top = a + (b - a) * tx, bottom = c + (d - c) * tx;
    return top + (bottom - top) * ty;
}
}

inline TerrainTexture makeTerrainTexture(std::uint64_t seed) {
    TerrainTexture out;
    out.rgba.resize(TerrainTexture::kMaxBytes);
    constexpr std::array<int, 5> cells{4, 8, 16, 32, 64};
    constexpr std::array<float, 5> weights{0.42f, 0.27f, 0.17f, 0.10f, 0.04f};
    constexpr float weightTotal = 1.0f;
    for (std::uint32_t y = 0; y < TerrainTexture::kSize; ++y) {
        for (std::uint32_t x = 0; x < TerrainTexture::kSize; ++x) {
            float grain = 0.0f;
            for (std::size_t i = 0; i < cells.size(); ++i) {
                grain += (terrain_texture_detail::octave(hashMix(seed + i), x, y, cells[i]) - 0.5f) * weights[i];
            }
            const float value = std::clamp(238.0f + (grain / weightTotal) * 52.0f, 218.0f, 255.0f);
            const std::size_t at = (std::size_t(y) * TerrainTexture::kSize + x) * 4;
            // Slightly warm neutral stone/soil; near-white values preserve existing biome tints.
            out.rgba[at + 0] = static_cast<std::uint8_t>(value);
            out.rgba[at + 1] = static_cast<std::uint8_t>(std::clamp(value - 3.0f, 0.0f, 255.0f));
            out.rgba[at + 2] = static_cast<std::uint8_t>(std::clamp(value - 7.0f, 0.0f, 255.0f));
            out.rgba[at + 3] = 255;
        }
    }
    return out;
}

} // namespace stalthos::domain
