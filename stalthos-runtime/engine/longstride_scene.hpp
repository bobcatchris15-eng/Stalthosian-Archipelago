#pragma once
#include "WickedEngine.h"
#include "domain/longstride.hpp"
#include <vector>
#include <cmath>
namespace stalthos::engine {
struct LongstridePresentation {wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;std::vector<wi::ecs::Entity> legs;};
inline LongstridePresentation longstrideScene(wi::scene::Scene& scene){using DirectX::XMFLOAT3;LongstridePresentation out;out.root=scene.Entity_CreateTransform("playable Longstride biped theropod");const auto root=out.root;
 auto part=[&](const char* name,XMFLOAT3 center,XMFLOAT3 radius,DirectX::XMFLOAT4 color,float tilt=0){
  std::vector<XMFLOAT3> v;std::vector<uint32_t> ix;
  for(uint32_t lat=0;lat<=8;++lat){const double t=lat*3.141592653589793/8;for(uint32_t lon=0;lon<=12;++lon){const double a=lon*6.283185307179586/12;v.push_back({float(std::sin(t)*std::cos(a)),float(std::cos(t)),float(std::sin(t)*std::sin(a))});}}
  for(uint32_t lat=0;lat<8;++lat)for(uint32_t lon=0;lon<12;++lon){const auto i=lat*13+lon;if(lat>0)ix.insert(ix.end(),{i,i+13,i+1});if(lat<7)ix.insert(ix.end(),{i+1,i+13,i+14});}
  const auto e=scene.Entity_CreateMeshFromData(name,ix.size(),ix.data(),v.size(),v.data());auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);for(const auto& sub:mesh->subsets){auto* m=scene.materials.GetComponent(sub.materialID);m->SetBaseColor(color);m->SetRoughness(.88f);}mesh->CreateRenderData();auto* tr=scene.transforms.GetComponent(e);tr->Scale(radius);tr->RotateRollPitchYaw(XMFLOAT3(tilt,0,0));tr->Translate(center);tr->UpdateTransform();scene.Component_Attach(e,root);return e;
 };
 const DirectX::XMFLOAT4 skin{.43f,.28f,.17f,1},dark{.08f,.065f,.04f,1},light{.66f,.51f,.32f,1};
 part("Longstride muscular torso",{0,1.10f,0},{.38f,.42f,.73f},skin,-.12f);
 part("Longstride strong shoulders",{0,1.24f,.48f},{.28f,.35f,.36f},skin);
 part("Longstride stout neck",{0,1.48f,.69f},{.24f,.33f,.27f},skin,.2f);
 part("Longstride deep blunt skull",{0,1.72f,.99f},{.28f,.28f,.39f},skin);
 part("Longstride broad muzzle",{0,1.62f,1.29f},{.25f,.19f,.26f},skin);
 part("Longstride dark jawline",{0,1.49f,1.22f},{.25f,.035f,.29f},dark);
 for(float x:{-.255f,.255f})part("Longstride eye",{x,1.81f,1.12f},{.024f,.028f,.034f},{.75f,.62f,.18f,1});
 for(float x:{-.31f,.31f}){out.legs.push_back(part("Longstride powerful thigh",{x,.70f,-.24f},{.20f,.40f,.25f},skin,-.2f));out.legs.push_back(part("Longstride lower leg",{x,.26f,-.14f},{.09f,.27f,.105f},skin,.2f));part("Longstride grounded foot",{x,.065f,.01f},{.14f,.065f,.28f},dark);}
 for(float x:{-.27f,.27f}){part("Longstride reduced forelimb",{x,1.10f,.58f},{.055f,.12f,.065f},skin,.3f);part("Longstride tiny hand",{x,1.00f,.63f},{.055f,.045f,.07f},dark);}
 part("Longstride stiff counterbalance tail",{0,1.02f,-.96f},{.22f,.22f,.68f},skin);
 part("Longstride tapering tail",{0,1.04f,-1.62f},{.115f,.12f,.55f},skin);
 for(int i=-2;i<=2;++i)part("Longstride dorsal marking",{0,1.45f,i*.24f},{.10f,.035f,.14f},light);
 return out;}
struct CarrionPresentation {wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;std::vector<wi::ecs::Entity> flesh;};
inline CarrionPresentation carrionScene(wi::scene::Scene& scene,const domain::CarrionPatch& patch,double ground){using DirectX::XMFLOAT3;CarrionPresentation out;out.root=scene.Entity_CreateTransform("authored carrion cohort - not a retained NPC death");const auto root=out.root;
 auto part=[&](const char* name,XMFLOAT3 center,XMFLOAT3 radius,DirectX::XMFLOAT4 color,float tilt=0){
  std::vector<XMFLOAT3> v;std::vector<uint32_t> ix;
  for(uint32_t lat=0;lat<=8;++lat){const double t=lat*3.141592653589793/8;for(uint32_t lon=0;lon<=12;++lon){const double a=lon*6.283185307179586/12;v.push_back({float(std::sin(t)*std::cos(a)),float(std::cos(t)),float(std::sin(t)*std::sin(a))});}}
  for(uint32_t lat=0;lat<8;++lat)for(uint32_t lon=0;lon<12;++lon){const auto i=lat*13+lon;if(lat>0)ix.insert(ix.end(),{i,i+13,i+1});if(lat<7)ix.insert(ix.end(),{i+1,i+13,i+14});}
  const auto e=scene.Entity_CreateMeshFromData(name,ix.size(),ix.data(),v.size(),v.data());auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);for(const auto& sub:mesh->subsets){auto* m=scene.materials.GetComponent(sub.materialID);m->SetBaseColor(color);m->SetRoughness(.88f);}mesh->CreateRenderData();auto* tr=scene.transforms.GetComponent(e);tr->Scale(radius);tr->RotateRollPitchYaw(XMFLOAT3(tilt,0,0));tr->Translate(center);tr->UpdateTransform();scene.Component_Attach(e,root);return e;
 };
 const DirectX::XMFLOAT4 bone{.67f,.61f,.43f,1},flesh{.35f,.12f,.075f,1};
 out.flesh.push_back(part("finite authored carrion flesh",{0,.21f,0},{.43f,.23f,.72f},flesh));
 out.flesh.push_back(part("finite authored carrion haunch",{-.33f,.16f,-.37f},{.24f,.18f,.28f},{.43f,.17f,.10f,1}));
 part("authored carrion backbone",{0,.19f,0},{.055f,.06f,.72f},bone);
 for(int side:{-1,1})for(int i=-2;i<=2;++i)part("authored carrion rib",{side*.25f,.20f,i*.22f},{.26f,.05f,.04f},bone,side*.3f);
 part("authored carrion skull",{0,.13f,.89f},{.22f,.13f,.26f},bone);
 for(int side:{-1,1})part("authored carrion limb bone",{side*.51f,.065f,-.43f},{.27f,.045f,.055f},bone);
 auto* tr=scene.transforms.GetComponent(root);tr->Translate(XMFLOAT3(float(patch.x),float(ground),float(patch.z)));tr->UpdateTransform();return out;}
}
