#pragma once
#include "WickedEngine.h"
#include "domain/brackenback.hpp"
#include <vector>
#include <cmath>
namespace stalthos::engine {
struct BrackenPresentation {wi::ecs::Entity root=wi::ecs::INVALID_ENTITY,club=wi::ecs::INVALID_ENTITY;std::vector<wi::ecs::Entity> legs,armor;};
inline BrackenPresentation brackenbackScene(wi::scene::Scene& scene){
 using DirectX::XMFLOAT3;BrackenPresentation out;out.root=scene.Entity_CreateTransform("playable Brackenback armored quadruped");const auto root=out.root;
 auto part=[&](const char* name,XMFLOAT3 center,XMFLOAT3 radius,DirectX::XMFLOAT4 color,float tilt=0){
  std::vector<XMFLOAT3> v;std::vector<uint32_t> ix;
  for(uint32_t lat=0;lat<=8;++lat){const double t=lat*3.141592653589793/8;for(uint32_t lon=0;lon<=12;++lon){const double a=lon*6.283185307179586/12;v.push_back({float(std::sin(t)*std::cos(a)),float(std::cos(t)),float(std::sin(t)*std::sin(a))});}}
  for(uint32_t lat=0;lat<8;++lat)for(uint32_t lon=0;lon<12;++lon){const auto i=lat*13+lon;if(lat>0)ix.insert(ix.end(),{i,i+13,i+1});if(lat<7)ix.insert(ix.end(),{i+1,i+13,i+14});}
  const auto e=scene.Entity_CreateMeshFromData(name,ix.size(),ix.data(),v.size(),v.data());auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);for(const auto& sub:mesh->subsets){auto* m=scene.materials.GetComponent(sub.materialID);m->SetBaseColor(color);m->SetRoughness(.88f);}mesh->CreateRenderData();auto* tr=scene.transforms.GetComponent(e);tr->Scale(radius);tr->RotateRollPitchYaw(XMFLOAT3(tilt,0,0));tr->Translate(center);tr->UpdateTransform();scene.Component_Attach(e,root);return e;
 };
 const DirectX::XMFLOAT4 skin{.29f,.32f,.17f,1},plate{.47f,.43f,.28f,1},dark{.07f,.065f,.035f,1};
 part("Brackenback low broad body",{0,.53f,0},{.55f,.35f,.83f},skin);
 part("Brackenback broad skull",{0,.36f,.88f},{.37f,.22f,.33f},skin);
 part("Brackenback low blunt beak",{0,.28f,1.10f},{.26f,.13f,.20f},{.38f,.33f,.19f,1});
 for(float x:{-.30f,.30f})part("Brackenback eye",{x,.44f,1.02f},{.025f,.028f,.032f},dark);
 for(float x:{-.39f,.39f})for(float z:{-.51f,.51f}){out.legs.push_back(part("Brackenback stout limb",{x,.22f,z},{.14f,.22f,.15f},skin));part("Brackenback grounded foot",{x,.07f,z+.045f},{.17f,.07f,.20f},dark);}
 part("Brackenback stiff tail",{0,.35f,-1.13f},{.11f,.11f,.57f},skin);
 out.club=part("Brackenback age-dependent tail club",{0,.36f,-1.67f},{.25f,.18f,.25f},plate);
 for(int row=-1;row<=1;++row)for(int column=-2;column<=2;++column){const float x=row*.30f,z=column*.27f,h=.80f-.06f*std::abs(row);out.armor.push_back(part("Brackenback dorsal osteoderm",{x,h,z},{.16f,.14f,.17f},plate));}
 for(int side:{-1,1})for(int i=-2;i<=2;++i)out.armor.push_back(part("Brackenback flank osteoderm",{side*.51f,.58f,i*.29f},{.17f,.10f,.15f},plate));
 return out;
}
}
