#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace stalthos::domain {

struct SurfaceVertex {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Row-vector matrix compatible with DirectX / Wicked Engine:
// [x y z 1] * M
struct Matrix4x4 {
    double m[4][4] = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };

    static Matrix4x4 identity() noexcept {
        return Matrix4x4{};
    }

    static Matrix4x4 translation(double tx, double ty, double tz) noexcept {
        Matrix4x4 mat;
        mat.m[3][0] = tx;
        mat.m[3][1] = ty;
        mat.m[3][2] = tz;
        return mat;
    }

    static Matrix4x4 scaling(double sx, double sy, double sz) noexcept {
        Matrix4x4 mat;
        mat.m[0][0] = sx;
        mat.m[1][1] = sy;
        mat.m[2][2] = sz;
        return mat;
    }

    static Matrix4x4 fromRowMajor(const float* f16) noexcept {
        Matrix4x4 mat;
        if (!f16) return mat;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                mat.m[r][c] = static_cast<double>(f16[r * 4 + c]);
            }
        }
        return mat;
    }

    static Matrix4x4 fromRowMajor(const double* d16) noexcept {
        Matrix4x4 mat;
        if (!d16) return mat;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                mat.m[r][c] = d16[r * 4 + c];
            }
        }
        return mat;
    }

    bool isFinite() const noexcept {
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                if (!std::isfinite(m[r][c])) return false;
            }
        }
        return true;
    }
};

inline SurfaceVertex transformPoint(const SurfaceVertex& p, const Matrix4x4& mat) noexcept {
    double w = p.x * mat.m[0][3] + p.y * mat.m[1][3] + p.z * mat.m[2][3] + mat.m[3][3];
    double invW = (std::isfinite(w) && w != 0.0 && w != 1.0) ? (1.0 / w) : 1.0;
    return SurfaceVertex{
        (p.x * mat.m[0][0] + p.y * mat.m[1][0] + p.z * mat.m[2][0] + mat.m[3][0]) * invW,
        (p.x * mat.m[0][1] + p.y * mat.m[1][1] + p.z * mat.m[2][1] + mat.m[3][1]) * invW,
        (p.x * mat.m[0][2] + p.y * mat.m[1][2] + p.z * mat.m[2][2] + mat.m[3][2]) * invW
    };
}

struct TriangleHit {
    int triangleIndexInCell = 0; // 0 for {a, b, c}, 1 for {b, d, c}
    uint32_t indices[3] = {0, 0, 0};
    double weights[3] = {0.0, 0.0, 0.0};
    double height = 0.0;
};

class RenderedGroundSurface {
public:
    RenderedGroundSurface() = default;

    template <typename VertexT>
    bool build(
        const VertexT* vertices, size_t vertexCount,
        const uint32_t* indices, size_t indexCount,
        const Matrix4x4& matrix = Matrix4x4::identity())
    {
        m_initialized = false;
        m_errorMessage.clear();
        m_worldVertices.clear();

        if (vertexCount > 262144 || indexCount > 1572864) { m_errorMessage = "rendered ground mesh exceeds bounded cache capacity"; return false; }
        if (matrix.m[0][3] != 0.0 || matrix.m[1][3] != 0.0 || matrix.m[2][3] != 0.0 || matrix.m[3][3] != 1.0) { m_errorMessage = "rendered ground transform must be affine"; return false; }
        if (!vertices || vertexCount == 0) {
            m_errorMessage = "empty or null vertices";
            return false;
        }
        if (!indices || indexCount == 0 || (indexCount % 3 != 0)) {
            m_errorMessage = "invalid or misaligned index buffer";
            return false;
        }
        if (!matrix.isFinite()) {
            m_errorMessage = "nonfinite instance transform matrix";
            return false;
        }

        // 1. Transform vertices to world coordinates and validate finite values
        m_worldVertices.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) {
            const auto& v = vertices[i];
            if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
                m_errorMessage = "mesh contains nonfinite vertex coordinates at index " + std::to_string(i);
                m_worldVertices.clear();
                return false;
            }
            SurfaceVertex transformed = transformPoint(SurfaceVertex{static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z)}, matrix);
            if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z)) {
                m_errorMessage = "transformed vertex coordinates nonfinite at index " + std::to_string(i);
                m_worldVertices.clear();
                return false;
            }
            m_worldVertices[i] = transformed;
        }

        // Validate index bounds
        for (size_t i = 0; i < indexCount; ++i) {
            if (indices[i] >= vertexCount) {
                m_errorMessage = "index " + std::to_string(indices[i]) + " exceeds vertex count " + std::to_string(vertexCount);
                m_worldVertices.clear();
                return false;
            }
        }

        // 2. Discover regular grid dimensions Nx and Nz
        // Row 0 has z constant and x increasing. The first vertex where z changes marks the end of row 0.
        size_t discoveredNx = 0;
        const double originX = m_worldVertices[0].x;
        const double originZ = m_worldVertices[0].z;

        for (size_t i = 1; i < vertexCount; ++i) {
            if (std::abs(m_worldVertices[i].z - originZ) > 1e-4) {
                discoveredNx = i;
                break;
            }
        }

        if (discoveredNx < 2) {
            m_errorMessage = "unable to determine regular grid width Nx (minimum 2 required)";
            m_worldVertices.clear();
            return false;
        }

        if (vertexCount % discoveredNx != 0) {
            m_errorMessage = "vertex count " + std::to_string(vertexCount) + " is not divisible by detected Nx " + std::to_string(discoveredNx);
            m_worldVertices.clear();
            return false;
        }

        const size_t discoveredNz = vertexCount / discoveredNx;
        if (discoveredNz < 2) {
            m_errorMessage = "detected grid height Nz is less than 2";
            m_worldVertices.clear();
            return false;
        }

        // Expected cell count
        const size_t cellCountX = discoveredNx - 1;
        const size_t cellCountZ = discoveredNz - 1;
        const size_t expectedIndexCount = cellCountX * cellCountZ * 6;

        if (indexCount != expectedIndexCount) {
            m_errorMessage = "index count " + std::to_string(indexCount) + " does not match expected regular grid triangle count " + std::to_string(expectedIndexCount);
            m_worldVertices.clear();
            return false;
        }

        // Grid spacing
        const double spacingX = m_worldVertices[1].x - originX;
        const double spacingZ = m_worldVertices[discoveredNx].z - originZ;

        if (!std::isfinite(spacingX) || !std::isfinite(spacingZ) || std::abs(spacingX) < 1e-6 || std::abs(spacingZ) < 1e-6) {
            m_errorMessage = "degenerate or zero grid spacing";
            m_worldVertices.clear();
            return false;
        }

        // Validate regular grid vertex positions and axis alignment
        for (size_t iz = 0; iz < discoveredNz; ++iz) {
            for (size_t ix = 0; ix < discoveredNx; ++ix) {
                const size_t vIdx = iz * discoveredNx + ix;
                const double expectedX = originX + static_cast<double>(ix) * spacingX;
                const double expectedZ = originZ + static_cast<double>(iz) * spacingZ;
                if (std::abs(m_worldVertices[vIdx].x - expectedX) > 1e-3 ||
                    std::abs(m_worldVertices[vIdx].z - expectedZ) > 1e-3) {
                    m_errorMessage = "grid vertex (" + std::to_string(ix) + ", " + std::to_string(iz) + ") deviates from regular grid mapping";
                    m_worldVertices.clear();
                    return false;
                }
            }
        }

        // 3. Validate cell topology and exact triangle pattern {a, b, c, b, d, c}
        for (size_t iz = 0; iz < cellCountZ; ++iz) {
            for (size_t ix = 0; ix < cellCountX; ++ix) {
                const size_t cellIdx = iz * cellCountX + ix;
                const size_t baseIndex = cellIdx * 6;
                const uint32_t a = static_cast<uint32_t>(iz * discoveredNx + ix);
                const uint32_t b = a + 1;
                const uint32_t c = a + static_cast<uint32_t>(discoveredNx);
                const uint32_t d = c + 1;

                if (indices[baseIndex + 0] != a || indices[baseIndex + 1] != b || indices[baseIndex + 2] != c ||
                    indices[baseIndex + 3] != b || indices[baseIndex + 4] != d || indices[baseIndex + 5] != c) {
                    m_errorMessage = "cell (" + std::to_string(ix) + ", " + std::to_string(iz) + ") violates {a,b,c,b,d,c} triangle topology";
                    m_worldVertices.clear();
                    return false;
                }
            }
        }

        // Cache grid metadata
        m_nx = discoveredNx;
        m_nz = discoveredNz;
        m_originX = originX;
        m_originZ = originZ;
        m_spacingX = spacingX;
        m_spacingZ = spacingZ;

        const double endX = originX + static_cast<double>(discoveredNx - 1) * spacingX;
        const double endZ = originZ + static_cast<double>(discoveredNz - 1) * spacingZ;
        m_minX = std::min(originX, endX);
        m_maxX = std::max(originX, endX);
        m_minZ = std::min(originZ, endZ);
        m_maxZ = std::max(originZ, endZ);

        m_initialized = true;
        return true;
    }

    template <typename VertexT>
    bool build(
        const std::vector<VertexT>& vertices,
        const std::vector<uint32_t>& indices,
        const Matrix4x4& matrix = Matrix4x4::identity())
    {
        return build(vertices.data(), vertices.size(), indices.data(), indices.size(), matrix);
    }

    bool inBounds(double wx, double wz) const noexcept {
        if (!m_initialized || !std::isfinite(wx) || !std::isfinite(wz)) return false;
        // Small epsilon tolerance for boundary points
        constexpr double eps = 1e-6;
        return (wx >= m_minX - eps) && (wx <= m_maxX + eps) &&
               (wz >= m_minZ - eps) && (wz <= m_maxZ + eps);
    }

    TriangleHit sampleTriangle(double wx, double wz) const {
        if (!m_initialized) {
            throw std::runtime_error("RenderedGroundSurface uninitialized: " + m_errorMessage);
        }
        if (!inBounds(wx, wz)) {
            throw std::out_of_range("sample coordinate outside rendered ground mesh bounds");
        }

        // Cell index calculation in normalized coordinate space
        const double uNorm = (wx - m_originX) / m_spacingX;
        const double vNorm = (wz - m_originZ) / m_spacingZ;

        int ix = static_cast<int>(std::floor(uNorm));
        int iz = static_cast<int>(std::floor(vNorm));

        const int maxIx = static_cast<int>(m_nx) - 2;
        const int maxIz = static_cast<int>(m_nz) - 2;

        ix = std::clamp(ix, 0, maxIx);
        iz = std::clamp(iz, 0, maxIz);

        const double cellX0 = m_originX + static_cast<double>(ix) * m_spacingX;
        const double cellZ0 = m_originZ + static_cast<double>(iz) * m_spacingZ;

        // Normalized offsets within cell [0, 1]
        const double u = std::clamp((wx - cellX0) / m_spacingX, 0.0, 1.0);
        const double v = std::clamp((wz - cellZ0) / m_spacingZ, 0.0, 1.0);

        const uint32_t a = static_cast<uint32_t>(iz * m_nx + ix);
        const uint32_t b = a + 1;
        const uint32_t c = a + static_cast<uint32_t>(m_nx);
        const uint32_t d = c + 1;

        const double ya = m_worldVertices[a].y;
        const double yb = m_worldVertices[b].y;
        const double yc = m_worldVertices[c].y;
        const double yd = m_worldVertices[d].y;

        TriangleHit hit;
        if (u + v <= 1.0) {
            // Triangle 0: {a, b, c}
            hit.triangleIndexInCell = 0;
            hit.indices[0] = a;
            hit.indices[1] = b;
            hit.indices[2] = c;
            hit.weights[0] = 1.0 - u - v;
            hit.weights[1] = u;
            hit.weights[2] = v;
            hit.height = hit.weights[0] * ya + hit.weights[1] * yb + hit.weights[2] * yc;
        } else {
            // Triangle 1: {b, d, c}
            hit.triangleIndexInCell = 1;
            hit.indices[0] = b;
            hit.indices[1] = d;
            hit.indices[2] = c;
            hit.weights[0] = 1.0 - v;
            hit.weights[1] = u + v - 1.0;
            hit.weights[2] = 1.0 - u;
            hit.height = hit.weights[0] * yb + hit.weights[1] * yd + hit.weights[2] * yc;
        }

        return hit;
    }

    double sampleHeight(double wx, double wz) const {
        return sampleTriangle(wx, wz).height;
    }

    std::optional<double> trySampleHeight(double wx, double wz) const noexcept {
        if (!inBounds(wx, wz)) return std::nullopt;
        try {
            return sampleHeight(wx, wz);
        } catch (...) {
            return std::nullopt;
        }
    }

    bool isInitialized() const noexcept { return m_initialized; }
    const std::string& errorMessage() const noexcept { return m_errorMessage; }
    size_t vertexCount() const noexcept { return m_worldVertices.size(); }
    size_t triangleCount() const noexcept { return m_initialized ? (m_nx - 1) * (m_nz - 1) * 2 : 0; }
    size_t gridNx() const noexcept { return m_nx; }
    size_t gridNz() const noexcept { return m_nz; }
    double minX() const noexcept { return m_minX; }
    double maxX() const noexcept { return m_maxX; }
    double minZ() const noexcept { return m_minZ; }
    double maxZ() const noexcept { return m_maxZ; }
    double spacingX() const noexcept { return m_spacingX; }
    double spacingZ() const noexcept { return m_spacingZ; }
    const std::vector<SurfaceVertex>& worldVertices() const noexcept { return m_worldVertices; }

private:
    bool m_initialized = false;
    std::string m_errorMessage;
    std::vector<SurfaceVertex> m_worldVertices;
    size_t m_nx = 0;
    size_t m_nz = 0;
    double m_originX = 0.0;
    double m_originZ = 0.0;
    double m_spacingX = 0.0;
    double m_spacingZ = 0.0;
    double m_minX = 0.0;
    double m_maxX = 0.0;
    double m_minZ = 0.0;
    double m_maxZ = 0.0;
};

} // namespace stalthos::domain
