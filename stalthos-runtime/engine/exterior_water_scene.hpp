#pragma once
#include "WickedEngine.h"
#include "domain/exterior_water.hpp"
namespace stalthos::engine {
inline wi::ecs::Entity exteriorWaterScene(wi::scene::Scene& scene,const fauna::ExteriorWaterSource& source,const std::function<double(double,double)>& ground,double surface){
 using DirectX::XMFLOAT3;std::vector<XMFLOAT3> vertices{{float(source.bankX),float(surface),float(source.centerZ)}};std::vector<uint32_t> indices;
 for(int i=0;i<48;++i){const double a=i*6.283185307179586/48;vertices.push_back({float(source.bankX+source.radius*std::cos(a)),float(surface),float(source.centerZ+source.radius*std::sin(a))});}for(uint32_t i=0;i<48;++i)indices.insert(indices.end(),{0,i+1,(i+1)%48+1});
 const auto water=scene.Entity_CreateMeshFromData("exterior shallow spring water",indices.size(),indices.data(),vertices.size(),vertices.data());const auto* object=scene.objects.GetComponent(water);auto* mesh=scene.meshes.GetComponent(object->meshID);for(const auto& subset:mesh->subsets){auto* m=scene.materials.GetComponent(subset.materialID);m->shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;m->SetBaseColor({.06f,.27f,.31f,.85f});m->SetRoughness(.12f);}mesh->CreateRenderData();
 vertices.clear();indices.clear();for(int i=0;i<48;++i){const double a=i*6.283185307179586/48;const double wobble=1+.07*std::sin(5*a)+.04*std::cos(9*a);for(int ring=0;ring<3;++ring){const double r=(source.radius+(ring==2?.65:ring==1?.30:0))*wobble;const double x=source.bankX+r*std::cos(a),z=source.centerZ+r*std::sin(a);vertices.push_back({float(x),float(ring==2?ground(x,z)+.015:surface+(ring==1?.12:-.10)),float(z)});}}
 for(uint32_t i=0;i<48;++i)for(uint32_t ring=0;ring<2;++ring){const auto a=i*3+ring,b=((i+1)%48)*3+ring;indices.insert(indices.end(),{a,b,a+1,b,b+1,a+1});}
 const auto rim=scene.Entity_CreateMeshFromData("grounded weathered spring stone rim",indices.size(),indices.data(),vertices.size(),vertices.data());const auto* rimObject=scene.objects.GetComponent(rim);auto* rimMesh=scene.meshes.GetComponent(rimObject->meshID);for(const auto& subset:rimMesh->subsets){auto* m=scene.materials.GetComponent(subset.materialID);m->SetBaseColor({.36f,.34f,.25f,1});m->SetRoughness(.94f);}rimMesh->CreateRenderData();return water;
}
}
