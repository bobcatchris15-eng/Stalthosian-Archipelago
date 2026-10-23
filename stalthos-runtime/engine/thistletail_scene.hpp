#pragma once
#include "WickedEngine.h"
#include "domain/thistletail.hpp"
#include <vector>
#include <cmath>
#include <functional>
namespace stalthos::engine {
struct BirdPresentation {wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;std::vector<wi::ecs::Entity> legs;};
inline BirdPresentation thistletailScene(wi::scene::Scene& scene){
 using DirectX::XMFLOAT3;BirdPresentation out;out.root=scene.Entity_CreateTransform("playable Thistletail feathered biped");const auto root=out.root;
 auto part=[&](const char* name,XMFLOAT3 center,XMFLOAT3 radius,DirectX::XMFLOAT4 color,float tilt=0){
  std::vector<XMFLOAT3> v;std::vector<uint32_t> ix;
  for(uint32_t lat=0;lat<=8;++lat){const double t=lat*3.141592653589793/8;for(uint32_t lon=0;lon<=12;++lon){const double a=lon*6.283185307179586/12;v.push_back({float(std::sin(t)*std::cos(a)),float(std::cos(t)),float(std::sin(t)*std::sin(a))});}}
  for(uint32_t lat=0;lat<8;++lat)for(uint32_t lon=0;lon<12;++lon){const auto i=lat*13+lon;if(lat>0)ix.insert(ix.end(),{i,i+13,i+1});if(lat<7)ix.insert(ix.end(),{i+1,i+13,i+14});}
  const auto e=scene.Entity_CreateMeshFromData(name,ix.size(),ix.data(),v.size(),v.data());auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);for(const auto& sub:mesh->subsets){auto* m=scene.materials.GetComponent(sub.materialID);m->SetBaseColor(color);m->SetRoughness(.88f);}mesh->CreateRenderData();auto* tr=scene.transforms.GetComponent(e);tr->Scale(radius);tr->RotateRollPitchYaw(XMFLOAT3(tilt,0,0));tr->Translate(center);tr->UpdateTransform();scene.Component_Attach(e,root);return e;
 };
 const DirectX::XMFLOAT4 feather{.32f,.23f,.12f,1},dark{.08f,.065f,.035f,1};
 part("bird rounded feather body",{0,.62f,0},{.25f,.29f,.37f},feather,-.25f);
 part("bird upright neck",{0,.83f,.26f},{.11f,.23f,.12f},feather,.25f);
 part("bird head",{0,1.02f,.34f},{.14f,.15f,.16f},feather);
 part("bird pointed beak",{0,1.0f,.52f},{.065f,.055f,.15f},{.58f,.45f,.22f,1});
 for(float x:{-.13f,.13f})part("bird eye",{x,1.06f,.40f},{.019f,.025f,.025f},dark);
 for(int side:{-1,1}){const float x=side*.23f;part("bird folded wing",{x,.63f,-.02f},{.07f,.20f,.28f},{.40f,.30f,.16f,1},-.35f);for(int i=0;i<5;++i)part("layered wing feather",{x+side*.018f,.66f-i*.035f,-.12f-i*.035f},{.045f,.035f,.19f},{.49f,.37f,.20f,1},-.25f);}
 for(int i=-2;i<=2;++i)part("bird tail feather",{i*.055f,.69f,-.52f},{.045f,.045f,float(.30-.025*std::abs(i))},{.26f,.19f,.10f,1},-.35f);
 for(float x:{-.14f,.14f}){out.legs.push_back(part("bird leg",{x,.23f,.02f},{.045f,.23f,.05f},{.62f,.43f,.21f,1}));for(int toe=-1;toe<=1;++toe)part("bird splayed toe",{x+toe*.045f,.035f,.14f},{.025f,.035f,.14f},{.49f,.34f,.16f,1});}
 return out;
}
inline std::vector<wi::ecs::Entity> microforageScene(wi::scene::Scene& scene,const domain::MicroforagePatch& patch,const std::function<double(double,double)>& ground){
 using DirectX::XMFLOAT3;std::vector<wi::ecs::Entity> objects;
 for(int i=0;i<36;++i){const double a=i*2.399963229728653,r=.8*std::sqrt((i+.5)/36),x=patch.x+r*std::sin(a),z=patch.z+r*std::cos(a);std::vector<XMFLOAT3> v{{float(x-.10),float(ground(x-.1,z)+.025),float(z)},{float(x),float(ground(x,z-.16)+.03),float(z-.16)},{float(x+.10),float(ground(x+.1,z)+.025),float(z)},{float(x),float(ground(x,z+.16)+.03),float(z+.16)}};uint32_t indices[]={0,1,2,0,2,3};const auto e=scene.Entity_CreateMeshFromData("microforage litter/seed cohort",6,indices,v.size(),v.data());auto* m=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);for(const auto& sub:m->subsets){auto* material=scene.materials.GetComponent(sub.materialID);material->SetBaseColor(i%3==0?DirectX::XMFLOAT4(.70f,.53f,.24f,1):DirectX::XMFLOAT4(.24f,.13f,.055f,1));material->SetRoughness(1);}m->CreateRenderData();objects.push_back(e);}
 return objects;
}
}
