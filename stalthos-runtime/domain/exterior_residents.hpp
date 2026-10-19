#pragma once
#include "fauna_population.hpp"
#include <functional>
namespace stalthos::fauna {
struct ExteriorResidentsLayout {std::array<Vec3,8> points{{{24,0,24},{48,0,24},{72,0,24},{96,0,24},{104,0,32},{116,0,18},{132,0,30},{144,0,20}}};double sampleMeters=.5,maxGrade=.6;};
struct ExteriorHabitat {std::vector<HabitatSite> sites;std::array<std::size_t,4> residents;double maxMeasuredGrade=0;std::size_t samples=0;};
inline ExteriorHabitat exteriorHabitat(std::vector<HabitatSite> original,const std::function<double(double,double)>& ground,const std::function<bool(double,double)>& wet,const ExteriorResidentsLayout& layout={}) {
 if(original.empty()||layout.sampleMeters<=0||layout.maxGrade<=0) throw std::invalid_argument("invalid exterior habitat layout");
 ExteriorHabitat result;result.sites=std::move(original);const auto old=result.sites.size();
 auto edge=[&](Vec3 a,Vec3 b,bool record) {const double length=std::hypot(b.x-a.x,b.z-a.z);const auto steps=std::max(1,int(std::ceil(length/layout.sampleMeters)));double previous=ground(a.x,a.z),maxGrade=0;for(int i=0;i<=steps;++i){double f=double(i)/steps,x=a.x+(b.x-a.x)*f,z=a.z+(b.z-a.z)*f,y=ground(x,z);if(wet(x,z)||!std::isfinite(y)) return false;if(i)maxGrade=std::max(maxGrade,std::abs(y-previous)/(length/steps));previous=y;}if(maxGrade>layout.maxGrade)return false;if(record){result.maxMeasuredGrade=std::max(result.maxMeasuredGrade,maxGrade);result.samples+=steps+1;}return true;};
 auto first=layout.points[0];first.y=ground(first.x,first.z);std::size_t join=old;double nearest=1e30;for(std::size_t i=0;i<old;++i){auto p=result.sites[i].position;double d=std::hypot(p.x-first.x,p.z-first.z);if(d<nearest&&edge(p,first,false)){join=i;nearest=d;}}if(join==old)throw std::runtime_error("no dry rendered route from watershed to exterior residents");
 for(auto point:layout.points){point.y=ground(point.x,point.z);result.sites.push_back({point,false,false,{}});}
 auto link=[&](std::size_t a,std::size_t b){if(!edge(result.sites[a].position,result.sites[b].position,true))throw std::runtime_error("exterior habitat edge exceeds dry rendered grade contract");result.sites[a].neighbors.push_back(b);result.sites[b].neighbors.push_back(a);};link(join,old);for(std::size_t i=1;i<layout.points.size();++i)link(old+i-1,old+i);for(std::size_t i=0;i<4;++i)result.residents[i]=old+4+i;return result;
}
inline Population withExteriorResidents(const Population& original,const ExteriorHabitat& habitat,const std::set<Identity>& excluded) {
 const auto baseline=original.snapshot();Population result(baseline.seed,baseline.region,habitat.sites,baseline.actors.size()+4,excluded);auto expanded=result.snapshot();for(std::size_t i=0;i<baseline.actors.size();++i) expanded.actors[i]=baseline.actors[i];
 for(std::size_t i=0;i<4;++i){auto& a=expanded.actors[baseline.actors.size()+i];a.site=habitat.residents[i];a.target=habitat.residents[(i+1)%4];a.position=habitat.sites[a.site].position;a.activity=Activity::Travel;}
 result.restore(expanded,excluded);return result;
}
}
