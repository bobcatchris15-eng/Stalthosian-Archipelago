#pragma once
#include "WickedEngine.h"
#include "domain/nesting.hpp"
#include <vector>
#include <functional>
namespace stalthos::engine {
// Replaceable authored-shape stand-in: a low earthen bowl, twig ring and three
// tapered eggs. No external asset, parenting rig or reproduction-success claim.
inline std::vector<wi::ecs::Entity> nestScene(wi::scene::Scene& scene,const domain::Clutch& clutch,const std::function<double(double,double)>& ground){
 using DirectX::XMFLOAT3;
 std::vector<wi::ecs::Entity> entities;
 auto mesh=[&](const char* name,const std::vector<XMFLOAT3>& v,const std::vector<uint32_t>& ix,DirectX::XMFLOAT4 color){
  const auto e=scene.Entity_CreateMeshFromData(name,ix.size(),ix.data(),v.size(),v.data());
  auto* m=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);
  for(const auto& s:m->subsets){auto* material=scene.materials.GetComponent(s.materialID);material->SetBaseColor(color);material->SetRoughness(.95f);}m->CreateRenderData();entities.push_back(e);
 };
 const auto p=clutch.position;
 std::vector<XMFLOAT3> v;std::vector<uint32_t> ix;
 constexpr unsigned segments=40;
 for(unsigned i=0;i<segments;++i){const double a=i*6.283185307179586/segments,wobble=1+.06*std::sin(a*7);for(unsigned ring=0;ring<4;++ring){const double r=(ring==0?.12:ring==1?.52:ring==2?.68:.88)*wobble,x=p.x+r*std::sin(a),z=p.z+r*std::cos(a);v.push_back({float(x),float(ground(x,z)+(ring==2?.16:ring==1?.055:.025)),float(z)});}}
 for(uint32_t i=0;i<segments;++i)for(uint32_t ring=0;ring<3;++ring){const auto a=i*4+ring,b=((i+1)%segments)*4+ring;ix.insert(ix.end(),{a,b,a+1,b,b+1,a+1});}
 mesh("Smalltrotter nest - replaceable earthen bowl",v,ix,{.29f,.20f,.105f,1});
 // Small crossed sticks distinguish this from a rock or resource marker.
 for(unsigned i=0;i<20;++i){const double a=i*6.283185307179586/20,r=.66+.06*std::sin(i*3.1),x=p.x+r*std::sin(a),z=p.z+r*std::cos(a);const auto e=scene.Entity_CreateCube("nest twig");auto* t=scene.transforms.GetComponent(e);t->Scale(XMFLOAT3(.025f,.025f,.22f));t->RotateRollPitchYaw(XMFLOAT3(.12f,float(a+.8),.1f));t->Translate(XMFLOAT3(float(x),float(ground(x,z)+.13),float(z)));t->UpdateTransform();auto* m=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);for(const auto& s:m->subsets){auto* mat=scene.materials.GetComponent(s.materialID);mat->SetBaseColor({.38f,.27f,.13f,1});mat->SetRoughness(1);}entities.push_back(e);}
 for(unsigned egg=0;egg<3;++egg){v.clear();ix.clear();const double a=egg*6.283185307179586/3,cx=p.x+.25*std::sin(a),cz=p.z+.25*std::cos(a),base=ground(cx,cz)+.045;
  for(unsigned lat=0;lat<=10;++lat){const double theta=lat*3.141592653589793/10,h=std::cos(theta),r=.17*std::sin(theta)*(1-.16*h);for(unsigned lon=0;lon<=16;++lon){const double phi=lon*6.283185307179586/16;v.push_back({float(cx+r*std::cos(phi)),float(base+.22+.22*h),float(cz+r*std::sin(phi))});}}
  for(uint32_t lat=0;lat<10;++lat)for(uint32_t lon=0;lon<16;++lon){const auto i=lat*17+lon;if(lat>0)ix.insert(ix.end(),{i,i+17,i+1});if(lat<9)ix.insert(ix.end(),{i+1,i+17,i+18});}
  mesh("Smalltrotter egg - incubation only",v,ix,{.79f,.74f,.56f,1});
 }
 return entities;
}
}
