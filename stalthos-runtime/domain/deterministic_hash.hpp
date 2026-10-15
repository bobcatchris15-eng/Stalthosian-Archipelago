#pragma once

#include <cstdint>

namespace stalthos::domain {

// Deterministic 64-bit splitmix hasher.
inline constexpr std::uint64_t hashMix(std::uint64_t x) noexcept {
    x += UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31U);
}

// Unit uniform real in [0.0, 1.0) derived from 64-bit hash.
inline constexpr double hashUnit(std::uint64_t v) noexcept {
    return static_cast<double>(v >> 11U) * (1.0 / 9007199254740992.0);
}

} // namespace stalthos::domain
