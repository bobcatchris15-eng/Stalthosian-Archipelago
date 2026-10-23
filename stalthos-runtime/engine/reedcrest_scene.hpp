#pragma once
#include "WickedEngine.h"
#include "domain/reedcrest.hpp"
#include <vector>
#include <cmath>
namespace stalthos::engine {
struct ReedPresentation {wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;std::vector<wi::ecs::Entity> legs;};
inline ReedPresentation reedcrestScene(wi::scene::Scene& scene){using DirectX::XMFLOAT3;ReedPresentation out;out.root=scene.Entity_CreateTransform("playable Reedcrest basal hadrosauroid");const auto root=out.root;
 auto part=[&](const char* name,XMFLOAT3 center,XMFLOAT3 radius,DirectX::XMFLOAT4 color,float tilt=0){
  std::vector<XMFLOAT3> v;std::vector<uint32_t> ix;
  for(uint32_t lat=0;lat<=8;++lat){const double t=lat*3.141592653589793/8;for(uint32_t lon=0;lon<=12;++lon){const double a=lon*6.283185307179586/12;v.push_back({float(std::sin(t)*std::cos(a)),float(std::cos(t)),float(std::sin(t)*std::sin(a))});}}
  for(uint32_t lat=0;lat<8;++lat)for(uint32_t lon=0;lon<12;++lon){const auto i=lat*13+lon;if(lat>0)ix.insert(ix.end(),{i,i+13,i+1});if(lat<7)ix.insert(ix.end(),{i+1,i+13,i+14});}
  const auto e=scene.Entity_CreateMeshFromData(name,ix.size(),ix.data(),v.size(),v.data());auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);for(const auto& sub:mesh->subsets){auto* m=scene.materials.GetComponent(sub.materialID);m->SetBaseColor(color);m->SetRoughness(.88f);}mesh->CreateRenderData();auto* tr=scene.transforms.GetComponent(e);tr->Scale(radius);tr->RotateRollPitchYaw(XMFLOAT3(tilt,0,0));tr->Translate(center);tr->UpdateTransform();scene.Component_Attach(e,root);return e;
 };
 const DirectX::XMFLOAT4 skin{.28f,.42f,.35f,1},light{.59f,.61f,.40f,1},dark{.075f,.09f,.06f,1};
 part("Reedcrest barrel torso",{0,.95f,0},{.44f,.46f,.80f},skin);
 part("Reedcrest sloping shoulder",{0,1.03f,.53f},{.30f,.36f,.36f},skin);
 part("Reedcrest alert short neck",{0,1.36f,.66f},{.20f,.40f,.21f},skin,.35f);
 part("Reedcrest broad head",{0,1.66f,.85f},{.24f,.23f,.35f},skin);
 part("Reedcrest broad beak",{0,1.56f,1.15f},{.29f,.11f,.26f},{.55f,.46f,.26f,1});
 for(float x:{-.22f,.22f})part("Reedcrest eye",{x,1.73f,1.0f},{.03f,.032f,.04f},dark);
 part("Reedcrest long counterbalance tail",{0,.93f,-1.02f},{.20f,.20f,.67f},skin);
 part("Reedcrest tapering tail tip",{0,.96f,-1.63f},{.09f,.09f,.42f},skin);
 for(float x:{-.33f,.33f}){out.legs.push_back(part("Reedcrest powerful hindlimb",{x,.46f,-.38f},{.17f,.46f,.20f},skin));part("Reedcrest hind foot",{x,.08f,-.26f},{.20f,.08f,.31f},dark);}
 for(float x:{-.27f,.27f}){out.legs.push_back(part("Reedcrest foraging forelimb",{x,.40f,.58f},{.095f,.40f,.11f},skin));part("Reedcrest forefoot",{x,.06f,.65f},{.12f,.06f,.17f},dark);}
 for(int side:{-1,1})for(int i=-2;i<=2;++i)part("Reedcrest pale flank marking",{side*.425f,1.05f,i*.25f},{.025f,.12f,.12f},light);
 return out;}
}
