#pragma once
#include "WickedEngine.h"
#include "domain/connected_route.hpp"
#include "organic_rocks.hpp"
namespace stalthos::engine {
inline std::size_t exteriorShoreScene(wi::scene::Scene& scene,const world::ConnectedRoute& route) {
 const auto& layout=route.shore().layout();
 // One finite static zero-level water sheet. Actual terrain occludes its dry banks.
 using DirectX::XMFLOAT3;std::vector<XMFLOAT3> vertices{{float(layout.startX),float(layout.seaLevel),-400},{800,float(layout.seaLevel),-400},{float(layout.startX),float(layout.seaLevel),448},{800,float(layout.seaLevel),448}};
 std::vector<uint32_t> indices{0,1,2,1,3,2};
 const auto water=scene.Entity_CreateMeshFromData("bounded exterior coastal cove water",indices.size(),indices.data(),vertices.size(),vertices.data());
 const auto* object=scene.objects.GetComponent(water);auto* mesh=scene.meshes.GetComponent(object->meshID);
 for(const auto& subset:mesh->subsets){auto* m=scene.materials.GetComponent(subset.materialID);m->shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;m->SetBaseColor({.035f,.22f,.28f,.92f});m->SetRoughness(.17f);m->SetDoubleSided(true);}mesh->CreateRenderData();
 const auto material=scene.Entity_CreateMaterial("coastal weathered grey rock");scene.materials.GetComponent(material)->SetBaseColor({.34f,.35f,.31f,1});scene.materials.GetComponent(material)->SetRoughness(.96f);
 std::size_t rocks=0;
 for(std::size_t i=0;i<14;++i){const double x=244+(i%7)*4,z=layout.centerZ+(i<7?-18:20)+3*std::sin(double(i));const auto geometry=detail::createOrganicRockMesh(domain::PlacementKind::Rock,.6+.18*(i%3),5100+i);const auto entity=scene.Entity_CreateMeshFromData("replaceable cove shore rock",geometry.indices.size(),geometry.indices.data(),geometry.positions.size(),geometry.positions.data());const auto* obj=scene.objects.GetComponent(entity);auto* rock=scene.meshes.GetComponent(obj->meshID);for(auto& subset:rock->subsets)subset.materialID=material;auto* transform=scene.transforms.GetComponent(entity);transform->Translate(DirectX::XMFLOAT3{float(x),float(route.triangleHeight(x,z)+geometry.groundOffset),float(z)});transform->UpdateTransform();rock->CreateRenderData();++rocks;}
 return rocks;
}
}
