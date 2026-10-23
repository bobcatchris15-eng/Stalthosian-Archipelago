#pragma once
#include "WickedEngine.h"
#include "domain/foundation.hpp"
#include <vector>
#include <cmath>
namespace stalthos::engine {
struct HatchlingPresentation {wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;std::vector<wi::ecs::Entity> legs;};
// Replaceable speculative Smalltrotter: long neck, distinct head, barrel body,
// tapering tail, four limbs and dark eyes. Dimensions are first-draft tuning.
inline HatchlingPresentation hatchlingScene(wi::scene::Scene& scene,Identity id){
 using DirectX::XMFLOAT3;HatchlingPresentation out;out.root=scene.Entity_CreateTransform("independent Smalltrotter hatchling "+std::to_string(id));
 auto part=[&](const char* name,XMFLOAT3 center,XMFLOAT3 radius,DirectX::XMFLOAT4 color,float tilt=0){
  std::vector<XMFLOAT3> v;std::vector<uint32_t> ix;
  for(uint32_t lat=0;lat<=8;++lat){const double t=lat*3.141592653589793/8;for(uint32_t lon=0;lon<=12;++lon){const double a=lon*6.283185307179586/12;v.push_back({float(std::sin(t)*std::cos(a)),float(std::cos(t)),float(std::sin(t)*std::sin(a))});}}
  for(uint32_t lat=0;lat<8;++lat)for(uint32_t lon=0;lon<12;++lon){const auto i=lat*13+lon;if(lat>0)ix.insert(ix.end(),{i,i+13,i+1});if(lat<7)ix.insert(ix.end(),{i+1,i+13,i+14});}
  const auto e=scene.Entity_CreateMeshFromData(name,ix.size(),ix.data(),v.size(),v.data());auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);for(const auto& sub:mesh->subsets){auto* m=scene.materials.GetComponent(sub.materialID);m->SetBaseColor(color);m->SetRoughness(.88f);}mesh->CreateRenderData();auto* tr=scene.transforms.GetComponent(e);tr->Scale(radius);tr->RotateRollPitchYaw(XMFLOAT3(tilt,0,0));tr->Translate(center);tr->UpdateTransform();scene.Component_Attach(e,out.root);return e;
 };
 const DirectX::XMFLOAT4 skin{.43f,.34f,.19f,1},dark{.09f,.075f,.04f,1};
 part("hatchling barrel body",{0,.47f,0},{.25f,.25f,.43f},skin);
 part("hatchling long neck",{0,.76f,.41f},{.09f,.35f,.10f},skin,.5f);
 part("hatchling head",{0,1.06f,.59f},{.12f,.12f,.18f},skin);
 part("hatchling muzzle",{0,1.025f,.73f},{.105f,.075f,.11f},{.50f,.42f,.27f,1});
 part("hatchling tapering tail base",{0,.48f,-.47f},{.12f,.12f,.27f},skin);
 part("hatchling tail tip",{0,.50f,-.76f},{.065f,.065f,.25f},skin);
 for(float x:{-.18f,.18f})for(float z:{-.25f,.25f}){out.legs.push_back(part("hatchling limb",{x,.18f,z},{.075f,.18f,.085f},skin));part("hatchling foot",{x,.055f,z+.025f},{.085f,.055f,.115f},dark);}
 for(float x:{-.112f,.112f})part("hatchling eye",{x,1.09f,.65f},{.018f,.024f,.024f},dark);
 return out;
}
}
