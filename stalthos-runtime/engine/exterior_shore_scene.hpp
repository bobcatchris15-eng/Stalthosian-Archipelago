#pragma once
#include "WickedEngine.h"
#include "domain/connected_route.hpp"
#include "organic_rocks.hpp"
namespace stalthos::engine {
inline std::size_t exteriorShoreScene(wi::scene::Scene& scene,const world::ConnectedRoute& route) {
 const auto& layout=route.shore().layout();
 // One finite static zero-level water sheet. Actual terrain occludes its dry banks.
 using DirectX::XMFLOAT3;std::vector<XMFLOAT3> vertices;std::vector<uint32_t> indices;
 const auto sheet=[&](float left,float right,float back,float front){const auto start=uint32_t(vertices.size());vertices.insert(vertices.end(),{{left,0,back},{right,0,back},{left,0,front},{right,0,front}});indices.insert(indices.end(),{start,start+1,start+2,start+1,start+3,start+2});};
 // Keep original reference watershed water untouched; sea covers all outer channels.
 sheet(-1400,-96,-1400,1400);sheet(96,1400,-1400,1400);sheet(-96,96,-1400,-96);sheet(-96,96,96,1400);
 const auto water=scene.Entity_CreateMeshFromData("bounded exterior coastal cove water",indices.size(),indices.data(),vertices.size(),vertices.data());
 const auto* object=scene.objects.GetComponent(water);auto* mesh=scene.meshes.GetComponent(object->meshID);
 for(const auto& subset:mesh->subsets){auto* m=scene.materials.GetComponent(subset.materialID);m->shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;m->SetBaseColor({.035f,.22f,.28f,.92f});m->SetRoughness(.17f);m->SetDoubleSided(true);}mesh->CreateRenderData();
 const auto material=scene.Entity_CreateMaterial("coastal weathered grey rock");scene.materials.GetComponent(material)->SetBaseColor({.34f,.35f,.31f,1});scene.materials.GetComponent(material)->SetRoughness(.96f);
 std::size_t rocks=0;
 for(std::size_t i=0;i<14;++i){const double x=244+(i%7)*4,z=layout.centerZ+(i<7?-18:20)+3*std::sin(double(i));const auto geometry=detail::createOrganicRockMesh(domain::PlacementKind::Rock,.6+.18*(i%3),5100+i);const auto entity=scene.Entity_CreateMeshFromData("replaceable cove shore rock",geometry.indices.size(),geometry.indices.data(),geometry.positions.size(),geometry.positions.data());const auto* obj=scene.objects.GetComponent(entity);auto* rock=scene.meshes.GetComponent(obj->meshID);for(auto& subset:rock->subsets)subset.materialID=material;auto* transform=scene.transforms.GetComponent(entity);transform->Translate(DirectX::XMFLOAT3{float(x),float(route.triangleHeight(x,z)+geometry.groundOffset),float(z)});transform->UpdateTransform();rock->CreateRenderData();++rocks;}
 return rocks;
}
}
