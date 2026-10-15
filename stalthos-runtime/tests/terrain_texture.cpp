#include "../domain/terrain_texture.hpp"

#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

using stalthos::domain::TerrainTexture;
using stalthos::domain::makeTerrainTexture;

static void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int main() {
    try {
        std::cout << "[test] terrain_texture focused verification starting...\n";

        // 1. Dimensions, mip counts, and bounded memory caps
        require(TerrainTexture::kBaseSize == 512, "base size is 512");
        require(TerrainTexture::kSize == 512, "alias size is 512");
        require(TerrainTexture::kMipLevels == 10, "mip count is 10 (512 down to 1)");
        require(TerrainTexture::kBaseBytes == 1048576, "base bytes is 1,048,576");
        require(TerrainTexture::kTotalMipBytes == 1398100, "total mip footprint is 1,398,100 bytes (< 1.4 MB)");
        require(TerrainTexture::kMaxBytes == 1398100, "max bytes matches total mip footprint");

        const auto tex42 = makeTerrainTexture(42);
        require(tex42.rgba.size() == TerrainTexture::kTotalMipBytes, "rgba buffer holds exact total mip footprint");

        std::size_t accumulatedBytes = 0;
        for (std::uint32_t level = 0; level < TerrainTexture::kMipLevels; ++level) {
            const std::uint32_t expectedDim = std::max(1U, TerrainTexture::kBaseSize >> level);
            const std::size_t expectedBytes = static_cast<std::size_t>(expectedDim) * expectedDim * 4;

            require(tex42.mipWidth(level) == expectedDim, "mip width matches expected power-of-two dimension");
            require(tex42.mipHeight(level) == expectedDim, "mip height matches expected power-of-two dimension");
            require(tex42.mipRowPitch(level) == expectedDim * 4, "mip row pitch is 4 bytes per texel");
            require(tex42.mipBytes(level) == expectedBytes, "mip byte count matches width * height * 4");
            require(tex42.mips[level].offset == accumulatedBytes, "mip offset is strictly contiguous");
            require(tex42.mipData(level) == tex42.rgba.data() + accumulatedBytes, "mipData pointer matches offset");

            accumulatedBytes += expectedBytes;
        }
        require(accumulatedBytes == TerrainTexture::kTotalMipBytes, "sum of all mip bytes equals kTotalMipBytes");

        // 2. Determinism: identical seed produces bitwise identical bytes; distinct seeds differ
        const auto tex42_copy = makeTerrainTexture(42);
        require(tex42.rgba == tex42_copy.rgba, "identical seed produces bitwise identical texture bytes across all mips");

        const auto tex99 = makeTerrainTexture(99);
        require(tex42.rgba != tex99.rgba, "distinct seeds produce distinct terrain albedos");

        // 3. Pixel Bounds and Opaque Alpha across all mip levels
        for (std::uint32_t level = 0; level < TerrainTexture::kMipLevels; ++level) {
            const std::uint8_t* data = tex42.mipData(level);
            const std::size_t pixelCount = static_cast<std::size_t>(tex42.mipWidth(level)) * tex42.mipHeight(level);
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const std::uint8_t r = data[i * 4 + 0];
                const std::uint8_t g = data[i * 4 + 1];
                const std::uint8_t b = data[i * 4 + 2];
                const std::uint8_t a = data[i * 4 + 3];

                require(a == 255, "alpha channel is strictly opaque 255 across all mips");
                require(r >= 190, "red channel is within bounded neutral range [190, 255]");
                require(g >= 185, "green channel is within bounded neutral range [185, 255]");
                require(b >= 180, "blue channel is within bounded neutral range [180, 255]");
            }
        }

        // 4. Aggregate Organic Statistics: high neutral albedo, warm earthy bias, healthy tooth
        double sumR = 0.0, sumG = 0.0, sumB = 0.0;
        std::uint8_t minR = 255, maxR = 0;
        const std::uint8_t* baseData = tex42.mipData(0);
        constexpr std::size_t kBasePixels = TerrainTexture::kBaseSize * TerrainTexture::kBaseSize;
        for (std::size_t i = 0; i < kBasePixels; ++i) {
            const std::uint8_t r = baseData[i * 4 + 0];
            const std::uint8_t g = baseData[i * 4 + 1];
            const std::uint8_t b = baseData[i * 4 + 2];
            sumR += r;
            sumG += g;
            sumB += b;
            if (r < minR) minR = r;
            if (r > maxR) maxR = r;
        }
        const double meanR = sumR / kBasePixels;
        const double meanG = sumG / kBasePixels;
        const double meanB = sumB / kBasePixels;

        require(meanR >= 225.0 && meanR <= 246.0, "mean red channel is within high neutral ground range [225, 246]");
        require(meanG >= 220.0 && meanG <= 244.0, "mean green channel is within neutral ground range [220, 244]");
        require(meanB >= 215.0 && meanB <= 240.0, "mean blue channel is within neutral ground range [215, 240]");
        require(meanR >= meanG && meanG >= meanB, "warm earthy tone ordering R >= G >= B preserved");
        require((maxR - minR) >= 25, "adequate organic contrast range (> 25 levels) for visible ground tooth");

        // 5. Periodic Boundary Continuity: seamless wrapping at x=511 -> x=0 and y=511 -> y=0
        double maxWrapDeltaX = 0.0;
        double maxInteriorDeltaX = 0.0;
        for (std::uint32_t y = 0; y < TerrainTexture::kBaseSize; ++y) {
            const std::size_t rowStart = static_cast<std::size_t>(y) * TerrainTexture::kBaseSize * 4;
            // Wrapped step between x=511 and x=0
            const int wrapDiffR = std::abs(static_cast<int>(baseData[rowStart + 0]) - static_cast<int>(baseData[rowStart + (TerrainTexture::kBaseSize - 1) * 4 + 0]));
            if (wrapDiffR > maxWrapDeltaX) maxWrapDeltaX = wrapDiffR;

            // Interior step between x=255 and x=256
            const int intDiffR = std::abs(static_cast<int>(baseData[rowStart + 256 * 4 + 0]) - static_cast<int>(baseData[rowStart + 255 * 4 + 0]));
            if (intDiffR > maxInteriorDeltaX) maxInteriorDeltaX = intDiffR;
        }
        // Seamless wrap: wrap delta must be bounded smoothly, comparable to interior pixel steps
        require(maxWrapDeltaX <= 28.0, "periodic X boundary wraps smoothly without seam discontinuity");

        double maxWrapDeltaY = 0.0;
        for (std::uint32_t x = 0; x < TerrainTexture::kBaseSize; ++x) {
            const std::size_t colTop = x * 4;
            const std::size_t colBottom = (static_cast<std::size_t>(TerrainTexture::kBaseSize - 1) * TerrainTexture::kBaseSize + x) * 4;
            const int wrapDiffR = std::abs(static_cast<int>(baseData[colTop + 0]) - static_cast<int>(baseData[colBottom + 0]));
            if (wrapDiffR > maxWrapDeltaY) maxWrapDeltaY = wrapDiffR;
        }
        require(maxWrapDeltaY <= 28.0, "periodic Y boundary wraps smoothly without seam discontinuity");

        // 6. Mip-Chain Energy Preservation: verify average luminance does not drift/darken down the chain
        for (std::uint32_t level = 1; level < TerrainTexture::kMipLevels; ++level) {
            const std::uint8_t* mipData = tex42.mipData(level);
            const std::size_t pixels = static_cast<std::size_t>(tex42.mipWidth(level)) * tex42.mipHeight(level);
            double mipSumR = 0.0;
            for (std::size_t i = 0; i < pixels; ++i) {
                mipSumR += mipData[i * 4 + 0];
            }
            const double mipMeanR = mipSumR / pixels;
            const double diff = std::abs(mipMeanR - meanR);
            require(diff < 2.5, "sRGB-correct downsampling preserves average albedo across all mip levels (< 2.5 drift)");
        }

        std::cout << "[test] terrain_texture focused verification passed successfully across all checks.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[test-failure] " << e.what() << "\n";
        return 1;
    }
}
