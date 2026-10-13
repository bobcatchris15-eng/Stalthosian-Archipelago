#include "../domain/rendered_ground_surface.hpp"

#include <iostream>
#include <cmath>
#include <limits>
#include <functional>
#include <string>

using namespace stalthos::domain;

static bool near(double a, double b, double epsilon = 1e-6) {
    return std::abs(a - b) < epsilon;
}

static void check(bool condition, const std::string& msg) {
    if (!condition) {
        std::cerr << "Assertion failed: " << msg << std::endl;
        throw std::runtime_error(msg);
    }
}

static void rejects(std::function<void()> f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception&) {
        caught = true;
    }
    if (!caught) {
        throw std::runtime_error("Expected exception was not thrown.");
    }
}

// Helper to generate a regular grid mesh with the standard {a,b,c,b,d,c} topology
struct TestGridMesh {
    std::vector<SurfaceVertex> vertices;
    std::vector<uint32_t> indices;
};

static TestGridMesh createGridMesh(
    size_t countX, size_t countZ,
    double originX, double originZ,
    double spacingX, double spacingZ,
    const std::function<double(double, double)>& heightFunc)
{
    TestGridMesh mesh;
    for (size_t z = 0; z < countZ; ++z) {
        for (size_t x = 0; x < countX; ++x) {
            double px = originX + static_cast<double>(x) * spacingX;
            double pz = originZ + static_cast<double>(z) * spacingZ;
            mesh.vertices.push_back({px, heightFunc(px, pz), pz});
        }
    }
    for (size_t z = 0; z < countZ - 1; ++z) {
        for (size_t x = 0; x < countX - 1; ++x) {
            uint32_t a = static_cast<uint32_t>(z * countX + x);
            uint32_t b = a + 1;
            uint32_t c = a + static_cast<uint32_t>(countX);
            uint32_t d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, c, b, d, c});
        }
    }
    return mesh;
}

// 1. Flat grid tests
static void testFlatGrid() {
    auto mesh = createGridMesh(5, 5, -10.0, -10.0, 5.0, 5.0, [](double, double) { return 3.5; });
    RenderedGroundSurface surface;
    check(surface.build(mesh.vertices, mesh.indices), "build flat grid should succeed");
    check(surface.isInitialized(), "flat grid should be initialized");
    check(surface.gridNx() == 5, "Nx should be 5");
    check(surface.gridNz() == 5, "Nz should be 5");
    check(near(surface.minX(), -10.0), "minX should be -10");
    check(near(surface.maxX(), 10.0), "maxX should be 10");
    check(near(surface.minZ(), -10.0), "minZ should be -10");
    check(near(surface.maxZ(), 10.0), "maxZ should be 10");

    // Sample across grid
    for (double z = -10.0; z <= 10.0; z += 1.25) {
        for (double x = -10.0; x <= 10.0; x += 1.25) {
            check(surface.inBounds(x, z), "all points inside boundary should be inBounds");
            double h = surface.sampleHeight(x, z);
            check(near(h, 3.5), "flat grid height should always be 3.5");
        }
    }
}

// 2. Sloped grid tests
static void testSlopedGrid() {
    // Plane: y = 0.5 * x - 0.25 * z + 2.0
    auto planeFunc = [](double x, double z) { return 0.5 * x - 0.25 * z + 2.0; };
    auto mesh = createGridMesh(4, 4, 0.0, 0.0, 2.0, 2.0, planeFunc);

    RenderedGroundSurface surface;
    check(surface.build(mesh.vertices, mesh.indices), "build sloped grid should succeed");

    // Every point on the sloped planar grid should interpolate to the exact planar height
    for (double z = 0.1; z <= 5.9; z += 0.3) {
        for (double x = 0.1; x <= 5.9; x += 0.3) {
            double expected = planeFunc(x, z);
            double actual = surface.sampleHeight(x, z);
            check(near(actual, expected, 1e-9), "barycentric interpolation on sloped plane must match exactly");
        }
    }
}

// 3. Cell triangles and diagonal test
static void testCellTrianglesAndDiagonal() {
    // Single cell: (0,0)=1.0, (2,0)=3.0, (0,2)=5.0, (2,2)=7.0
    // Triangle 0: {a(0,0), b(2,0), c(0,2)} with heights {1, 3, 5}
    // Triangle 1: {b(2,0), d(2,2), c(0,2)} with heights {3, 7, 5}
    // Diagonal edge is between b(2,0) and c(0,2), corresponding to u + v == 1
    TestGridMesh mesh;
    mesh.vertices = {
        {0.0, 1.0, 0.0}, // a
        {2.0, 3.0, 0.0}, // b
        {0.0, 5.0, 2.0}, // c
        {2.0, 7.0, 2.0}  // d
    };
    mesh.indices = {0, 1, 2, 1, 3, 2};

    RenderedGroundSurface surface;
    check(surface.build(mesh.vertices, mesh.indices), "build 2x2 grid should succeed");

    // Point in Triangle 0: u = 0.25, v = 0.25 -> (x, z) = (0.5, 0.5)
    // u + v = 0.5 <= 1
    // wa = 1 - 0.5 = 0.5, wb = 0.25, wc = 0.25
    // height = 0.5 * 1.0 + 0.25 * 3.0 + 0.25 * 5.0 = 0.5 + 0.75 + 1.25 = 2.5
    auto hit0 = surface.sampleTriangle(0.5, 0.5);
    check(hit0.triangleIndexInCell == 0, "point (0.5, 0.5) should hit triangle 0");
    check(near(hit0.weights[0], 0.5), "weight a should be 0.5");
    check(near(hit0.weights[1], 0.25), "weight b should be 0.25");
    check(near(hit0.weights[2], 0.25), "weight c should be 0.25");
    check(near(hit0.height, 2.5), "height in triangle 0 should be 2.5");

    // Point in Triangle 1: u = 0.75, v = 0.75 -> (x, z) = (1.5, 1.5)
    // u + v = 1.5 > 1
    // wb = 1 - 0.75 = 0.25, wd = 1.5 - 1 = 0.5, wc = 1 - 0.75 = 0.25
    // height = 0.25 * 3.0 + 0.5 * 7.0 + 0.25 * 5.0 = 0.75 + 3.5 + 1.25 = 5.5
    auto hit1 = surface.sampleTriangle(1.5, 1.5);
    check(hit1.triangleIndexInCell == 1, "point (1.5, 1.5) should hit triangle 1");
    check(near(hit1.weights[0], 0.25), "weight b should be 0.25");
    check(near(hit1.weights[1], 0.5), "weight d should be 0.5");
    check(near(hit1.weights[2], 0.25), "weight c should be 0.25");
    check(near(hit1.height, 5.5), "height in triangle 1 should be 5.5");

    // Point exactly on the diagonal: u = 0.5, v = 0.5 -> (x, z) = (1.0, 1.0)
    // u + v = 1.0
    // Triangle 0: wa = 0, wb = 0.5, wc = 0.5 -> height = 0.5*3 + 0.5*5 = 4.0
    // Triangle 1: wd = 0, wb = 0.5, wc = 0.5 -> height = 0.5*3 + 0.5*5 = 4.0
    double diagHeight = surface.sampleHeight(1.0, 1.0);
    check(near(diagHeight, 4.0), "diagonal height must evaluate continuously to 4.0");

    // Additional diagonal points
    // (x=0.4, z=1.6) -> u = 0.2, v = 0.8 -> u + v = 1.0 -> 0.2*3 + 0.8*5 = 0.6 + 4.0 = 4.6
    check(near(surface.sampleHeight(0.4, 1.6), 4.6), "diagonal point (0.4, 1.6) should be 4.6");
}

// 4. Translated and scaled grid tests
static void testTranslatedAndScaledGrid() {
    auto mesh = createGridMesh(3, 3, 0.0, 0.0, 1.0, 1.0, [](double x, double z) { return x + z; });

    // Translation (tx=10, ty=5, tz=-20), Scale (sx=2, sy=1, sz=3)
    Matrix4x4 mat = Matrix4x4::identity();
    mat.m[0][0] = 2.0;
    mat.m[1][1] = 1.0;
    mat.m[2][2] = 3.0;
    mat.m[3][0] = 10.0;
    mat.m[3][1] = 5.0;
    mat.m[3][2] = -20.0;

    RenderedGroundSurface surface;
    check(surface.build(mesh.vertices, mesh.indices, mat), "build with transform matrix should succeed");

    check(near(surface.minX(), 10.0), "transformed minX should be 10");
    check(near(surface.maxX(), 14.0), "transformed maxX should be 14");
    check(near(surface.minZ(), -20.0), "transformed minZ should be -20");
    check(near(surface.maxZ(), -14.0), "transformed maxZ should be -14");
    check(near(surface.spacingX(), 2.0), "transformed spacingX should be 2.0");
    check(near(surface.spacingZ(), 3.0), "transformed spacingZ should be 3.0");

    // Local vertex (0, 0) height was 0.0 -> world height 0.0 + 5.0 = 5.0 at (10, -20)
    check(near(surface.sampleHeight(10.0, -20.0), 5.0), "origin vertex transformed height should be 5.0");
    // Local vertex (1, 1) was height 2.0 -> world height 2.0 + 5.0 = 7.0 at (12, -17)
    check(near(surface.sampleHeight(12.0, -17.0), 7.0), "center vertex transformed height should be 7.0");
}

// 5. Bounds and rejection
static void testBoundsAndRejection() {
    auto mesh = createGridMesh(3, 3, 0.0, 0.0, 2.0, 2.0, [](double, double) { return 1.0; });
    RenderedGroundSurface surface;
    surface.build(mesh.vertices, mesh.indices);

    check(surface.inBounds(0.0, 0.0), "(0,0) inBounds");
    check(surface.inBounds(4.0, 4.0), "(4,4) inBounds");
    check(!surface.inBounds(-0.1, 0.0), "(-0.1, 0) out of bounds");
    check(!surface.inBounds(0.0, 4.1), "(0, 4.1) out of bounds");
    check(!surface.inBounds(5.0, 2.0), "(5, 2) out of bounds");

    rejects([&]() { surface.sampleHeight(-1.0, 2.0); });
    rejects([&]() { surface.sampleHeight(2.0, 10.0); });
    rejects([&]() { surface.sampleHeight(std::numeric_limits<double>::quiet_NaN(), 0.0); });
    rejects([&]() { surface.sampleHeight(0.0, std::numeric_limits<double>::infinity()); });

    check(!surface.trySampleHeight(-1.0, 2.0).has_value(), "trySampleHeight out of bounds returns nullopt");
    check(surface.trySampleHeight(2.0, 2.0).has_value(), "trySampleHeight in bounds returns value");
}

// 6. Invalid and degenerate meshes
static void testInvalidAndDegenerateMeshes() {
    RenderedGroundSurface surface;

    // Empty vertices
    check(!surface.build(std::vector<SurfaceVertex>{}, std::vector<uint32_t>{0, 1, 2}), "empty vertices should fail");

    // Nonfinite vertices
    auto mesh = createGridMesh(3, 3, 0.0, 0.0, 1.0, 1.0, [](double, double) { return 0.0; });
    mesh.vertices[4].y = std::numeric_limits<double>::quiet_NaN();
    check(!surface.build(mesh.vertices, mesh.indices), "NaN vertex should fail");

    mesh.vertices[4].y = std::numeric_limits<double>::infinity();
    check(!surface.build(mesh.vertices, mesh.indices), "Inf vertex should fail");

    // Index out of bounds
    mesh = createGridMesh(3, 3, 0.0, 0.0, 1.0, 1.0, [](double, double) { return 0.0; });
    mesh.indices[0] = 999;
    check(!surface.build(mesh.vertices, mesh.indices), "out of bounds index should fail");

    // Incomplete index buffer
    mesh = createGridMesh(3, 3, 0.0, 0.0, 1.0, 1.0, [](double, double) { return 0.0; });
    mesh.indices.pop_back();
    check(!surface.build(mesh.vertices, mesh.indices), "incomplete index buffer should fail");

    // Degenerate zero spacing
    mesh = createGridMesh(3, 3, 0.0, 0.0, 0.0, 1.0, [](double, double) { return 0.0; });
    check(!surface.build(mesh.vertices, mesh.indices), "zero dx should fail");

    // Non-regular grid coords (perturbed coordinate)
    mesh = createGridMesh(3, 3, 0.0, 0.0, 1.0, 1.0, [](double, double) { return 0.0; });
    mesh.vertices[1].x += 0.5; // corrupt column spacing
    check(!surface.build(mesh.vertices, mesh.indices), "non-regular grid coordinates should fail");

    // Wrong topology: flipped indices
    mesh = createGridMesh(3, 3, 0.0, 0.0, 1.0, 1.0, [](double, double) { return 0.0; });
    std::swap(mesh.indices[0], mesh.indices[1]);
    check(!surface.build(mesh.vertices, mesh.indices), "swapped indices should fail topology validation");

    // Nonfinite matrix
    mesh = createGridMesh(3, 3, 0.0, 0.0, 1.0, 1.0, [](double, double) { return 0.0; });
    Matrix4x4 badMat = Matrix4x4::identity();
    badMat.m[0][0] = std::numeric_limits<double>::quiet_NaN();
    check(!surface.build(mesh.vertices, mesh.indices, badMat), "NaN matrix should fail");
}

int main() {
    try {
        std::cout << "Running RenderedGroundSurface test suite..." << std::endl;
        {
            auto mesh = createGridMesh(3, 3, 0.0, 0.0, 1.0, 1.0, [](double, double) { return 0.0; });
            RenderedGroundSurface surface;
            Matrix4x4 projective = Matrix4x4::identity(); projective.m[3][3] = 0.0;
            check(!surface.build(mesh.vertices, mesh.indices, projective), "projective zero w rejected");
            check(!surface.build(mesh.vertices.data(), 262145, mesh.indices.data(), mesh.indices.size()), "oversized cache rejected before dereference");
        }
        testFlatGrid();
        testSlopedGrid();
        testCellTrianglesAndDiagonal();
        testTranslatedAndScaledGrid();
        testBoundsAndRejection();
        testInvalidAndDegenerateMeshes();
        std::cout << "All RenderedGroundSurface tests PASSED successfully." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test suite FAILED with error: " << e.what() << std::endl;
        return 1;
    }
}
