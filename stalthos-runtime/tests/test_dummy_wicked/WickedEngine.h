#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <memory>

namespace DirectX {
    struct XMFLOAT3 {
        float x, y, z;
        XMFLOAT3() : x(0), y(0), z(0) {}
        XMFLOAT3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    };
    struct XMFLOAT4 {
        float x, y, z, w;
        XMFLOAT4() : x(0), y(0), z(0), w(0) {}
        XMFLOAT4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    };

    typedef XMFLOAT4 XMVECTOR;

    inline XMVECTOR XMQuaternionIdentity() { return XMVECTOR(0,0,0,1); }
    inline XMVECTOR XMQuaternionRotationRollPitchYaw(float /*pitch*/, float /*yaw*/, float /*roll*/) { return XMVECTOR(0,0,0,1); }
    inline XMVECTOR XMQuaternionMultiply(const XMVECTOR& /*q1*/, const XMVECTOR& /*q2*/) { return XMVECTOR(0,0,0,1); }
    inline XMVECTOR XMQuaternionNormalize(const XMVECTOR& /*q*/) { return XMVECTOR(0,0,0,1); }
    inline XMVECTOR XMLoadFloat3(const XMFLOAT3* p) { return XMVECTOR(p->x, p->y, p->z, 0); }
    inline XMVECTOR XMVector3Rotate(const XMVECTOR& v, const XMVECTOR& /*q*/) { return v; }
    inline void XMStoreFloat3(XMFLOAT3* p, const XMVECTOR& v) { p->x = v.x; p->y = v.y; p->z = v.z; }
}

namespace wi {
    struct Color {
        uint32_t rgba;
        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
            rgba = (a << 24) | (b << 16) | (g << 8) | r;
        }
    };

    namespace ecs {
        typedef uint32_t Entity;
        const Entity INVALID_ENTITY = 0;
        inline Entity CreateEntity() { static Entity id = 1; return id++; }

        template<typename T>
        struct ComponentManager {
            std::vector<T> components;
            T& Create(Entity /*e*/) { components.push_back(T{}); return components.back(); }
            T* GetComponent(Entity /*e*/) {
                if (components.empty()) {
                    components.push_back(T{});
                }
                return &components.back();
            }
            std::string& CreateString(Entity /*e*/) { static std::string s; return s; }
        };
    }

    namespace graphics {
        inline void* GetDevice() { return nullptr; }
    }

    namespace scene {
        struct NameComponent {
            std::string name;
            std::string& operator=(const std::string& n) { name = n; return name; }
        };

        struct LayerComponent {};

        struct TransformComponent {
            void ClearTransform() {}
            void RotateRollPitchYaw(const DirectX::XMFLOAT3& /*rot*/) {}
            void Translate(const DirectX::XMFLOAT3& /*trans*/) {}
            void UpdateTransform() {}
        };

        struct ObjectComponent {
            wi::ecs::Entity meshID;
        };

        struct MeshComponent {
            std::vector<DirectX::XMFLOAT3> vertex_positions;
            std::vector<uint32_t> indices;
            std::vector<uint32_t> vertex_colors;

            struct MeshSubset {
                uint32_t indexCount;
                wi::ecs::Entity materialID;
            };
            std::vector<MeshSubset> subsets;

            enum COMPUTE_NORMALS { COMPUTE_NORMALS_SMOOTH_FAST };
            void ComputeNormals(COMPUTE_NORMALS /*unused*/) {}
            void CreateRenderData() {}
        };

        struct MaterialComponent {
            void SetBaseColor(const DirectX::XMFLOAT4& /*color*/) {}
            void SetRoughness(float /*r*/) {}
            void SetMetalness(float /*m*/) {}
            void SetUseVertexColors(bool /*use*/) {}
            void SetDoubleSided(bool /*use*/) {}
        };

        struct Scene {
            struct { std::string& Create(wi::ecs::Entity /*e*/) { static std::string s; return s; } } names;
            wi::ecs::ComponentManager<LayerComponent> layers;
            wi::ecs::ComponentManager<TransformComponent> transforms;
            wi::ecs::ComponentManager<ObjectComponent> objects;
            wi::ecs::ComponentManager<MeshComponent> meshes;
            wi::ecs::ComponentManager<MaterialComponent> materials;

            wi::ecs::Entity Entity_CreateMaterial(const std::string& /*name*/) {
                wi::ecs::Entity e = wi::ecs::CreateEntity();
                materials.Create(e);
                return e;
            }
        };
    }
}
