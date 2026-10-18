#pragma once
#include "WickedEngine.h"
#include "domain/fauna_population.hpp"
#include "domain/reference_landscape.hpp"
#include "domain/rendered_ground_surface.hpp"
#include <functional>
#include <queue>
#include <string>
#include <vector>

void ImportModel_GLTF(const std::string&, wi::scene::Scene&);

namespace stalthos::engine {

// Presentation consumes actor projections; it never chooses their behavior.
class FaunaScene {
    struct Replica {
        wi::ecs::Entity root = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity walk = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity idle = wi::ecs::INVALID_ENTITY;
        double scale = 1;
    };
    domain::RenderedGroundSurface ground_;
    std::vector<Replica> replicas_;
    std::uint64_t groundedUpdates_ = 0;
    bool importedMeshes_ = false;
public:
    void cacheGround(wi::scene::Scene& scene) {
        const auto entity = scene.Entity_FindByName("draft reference ground");
        const auto* object = scene.objects.GetComponent(entity);
        const auto* mesh = object ? scene.meshes.GetComponent(object->meshID) : nullptr;
        const auto index = scene.objects.GetIndex(entity);
        if (!mesh || index >= scene.matrix_objects.size()) throw std::runtime_error("fauna requires rendered terrain mesh and instance matrix");
        const auto matrix = domain::Matrix4x4::fromRowMajor(reinterpret_cast<const float*>(&scene.matrix_objects[index]));
        if (!ground_.build(mesh->vertex_positions.data(), mesh->vertex_positions.size(), mesh->indices.data(), mesh->indices.size(), matrix))
            throw std::runtime_error("fauna terrain cache: " + ground_.errorMessage());
    }
    double height(double x, double z) const { return ground_.sampleHeight(x,z); }
    std::uint64_t groundedUpdates() const { return groundedUpdates_; }

    std::vector<fauna::HabitatSite> habitat(const domain::ReferenceLandscape& land, const std::vector<Vec3>& food) const {
        std::vector<fauna::HabitatSite> sites;
        // Find dry banks from the water mask itself, without duplicating the
        // private creek-center formula. Food positions come from actual shrubs.
        for (double z : {-10.0, -3.0, 4.0, 11.0}) {
            double bank = 30;
            bool found = false;
            for (double x=30; x>=-20; x-=0.25) {
                if (land.waterContains(x,z)) { bank=x+0.8; found=true; break; }
            }
            if (!found) continue;
            for (double offset : {0.0,4.0,8.0}) {
                const double x=bank+offset;
                if (land.waterContains(x,z)) continue;
                const bool water = offset==0 && land.waterContains(x-1.0,z);
                sites.push_back({{x,height(x,z),z},false,water,{}});
            }
        }
        for (const auto& p:food) if (land.inBounds(p.x,p.z) && !land.waterContains(p.x,p.z))
            sites.push_back({{p.x,height(p.x,p.z),p.z},true,false,{}});
        for (std::size_t i=0;i<sites.size();++i) for (std::size_t j=i+1;j<sites.size();++j) {
            const auto a=sites[i].position, b=sites[j].position;
            const double length=std::hypot(b.x-a.x,b.z-a.z);
            if (length<0.05 || length>16) continue;
            bool dry=true;
            const int steps=static_cast<int>(std::ceil(length/.35));
            double previous=height(a.x,a.z);
            for(int k=1;k<=steps;++k) {
                const double t=double(k)/steps, x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t;
                const double y=height(x,z);
                if (land.waterContains(x,z) || std::abs(y-previous)>0.45) { dry=false; break; }
                previous=y;
            }
            if(dry) { sites[i].neighbors.push_back(j); sites[j].neighbors.push_back(i); }
        }
        // Retain the largest connected component containing both resources.
        std::vector<bool> seen(sites.size(),false);
        std::vector<std::size_t> best;
        for(std::size_t start=0;start<sites.size();++start) {
            if(seen[start]) continue;
            std::vector<std::size_t> component{start}; seen[start]=true;
            bool browse=false,water=false;
            for(std::size_t q=0;q<component.size();++q) {
                const auto at=component[q]; browse|=sites[at].browse; water|=sites[at].freshwater;
                for(auto n:sites[at].neighbors) if(!seen[n]) {seen[n]=true; component.push_back(n);}
            }
            if(browse && water && component.size()>best.size()) best=component;
        }
        if(best.empty()) throw std::runtime_error("no connected dry fauna habitat links food and freshwater");
        std::vector<std::size_t> remap(sites.size(),sites.size());
        for(std::size_t i=0;i<best.size();++i) remap[best[i]]=i;
        std::vector<fauna::HabitatSite> result;
        for(auto old:best) { auto site=sites[old]; site.neighbors.clear(); for(auto n:sites[old].neighbors) if(remap[n]<best.size()) site.neighbors.push_back(remap[n]); result.push_back(std::move(site)); }
        return result;
    }

    void load(wi::scene::Scene& scene, const std::string& path, const std::vector<fauna::ActorView>& actors,
              const std::function<wi::ecs::Entity(std::size_t)>& proxy) {
        wi::scene::Scene source;
        importedMeshes_=!path.empty();
        if (!path.empty()) {
            ImportModel_GLTF(path,source);
            if(source.objects.GetCount()==0) throw std::runtime_error("fauna mesh import is empty");
            for(std::size_t i=0;i<source.animations.GetCount();++i) {
                auto& animation=source.animations[i];
                // The actor frame owns locomotion, not the imported non-skin root.
                std::set<wi::ecs::Entity> rootTranslations;
                for(const auto& ch:animation.channels) {
                    const bool rootTranslation=[&](){
                    if(ch.path!=wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION) return false;
                    for(std::size_t a=0;a<source.armatures.GetCount();++a) for(auto bone:source.armatures[a].boneCollection) if(bone==ch.target) return false;
                    for(const auto& other:animation.channels) if(other.target!=ch.target && !source.Entity_IsDescendant(other.target,ch.target)) return false;
                    return true;
                    }();
                    if(rootTranslation) rootTranslations.insert(ch.target);
                }
                animation.channels.erase(std::remove_if(animation.channels.begin(),animation.channels.end(),[&](const auto& ch){
                    return ch.path==wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION && rootTranslations.count(ch.target)>0;
                }),animation.channels.end());
            }
        }
        for(std::size_t i=0;i<actors.size();++i) {
            Replica replica;
            replica.scale=0.85+double(actors[i].id%21)/100.0;
            if(path.empty()) replica.root=proxy(i);
            else {
                replica.root=scene.Entity_CreateTransform("ambient fauna " + std::to_string(actors[i].id));
                const auto first=scene.animations.GetCount();
                const auto imported=scene.Instantiate(source,true);
                if(imported==wi::ecs::INVALID_ENTITY) throw std::runtime_error("fauna clone failed");
                scene.Component_Attach(imported,replica.root);
                for(std::size_t k=first;k<scene.animations.GetCount();++k) {
                    auto& a=scene.animations[k]; a.SetLooped(true); a.Stop(); a.amount=0;
                    const auto e=scene.animations.GetEntity(k); const auto* name=scene.names.GetComponent(e);
                    if(name && (name->name.find("Walk")!=std::string::npos || name->name.find("walk")!=std::string::npos)) replica.walk=e;
                    else if(name && (name->name.find("Idle")!=std::string::npos || name->name.find("idle")!=std::string::npos)) replica.idle=e;
                }
            }
            replicas_.push_back(replica);
        }
    }
    void update(wi::scene::Scene& scene,const std::vector<fauna::ActorView>& actors,double clipDuration,double stride) {
        if(actors.size()!=replicas_.size()) throw std::runtime_error("fauna projection/renderer count mismatch");
        for(std::size_t i=0;i<actors.size();++i) {
            const auto& a=actors[i]; const auto& r=replicas_[i];
            auto* transform=scene.transforms.GetComponent(r.root);
            if(!transform) throw std::runtime_error("fauna root missing");
            const auto y=height(a.position.x,a.position.z);
            if(!std::isfinite(y)) throw std::runtime_error("fauna ground elevation invalid");
            transform->translation_local={float(a.position.x),float(y),float(a.position.z)};
            transform->scale_local={float(r.scale),float(r.scale),float(r.scale)};
            transform->rotation_local=DirectX::XMFLOAT4{};
            DirectX::XMStoreFloat4(&transform->rotation_local,DirectX::XMQuaternionRotationRollPitchYaw(0,float(a.headingRadians-(importedMeshes_?1.5707963267948966:0.0)),0));
            transform->SetDirty(); transform->UpdateTransform(); ++groundedUpdates_;
            const bool travel=a.alive && a.activity==fauna::Activity::Travel;
            auto* walk=scene.animations.GetComponent(r.walk);
            auto* idle=scene.animations.GetComponent(r.idle);
            if(walk) {
                walk->Pause(); walk->amount=(travel || !idle)?1.0f:0.0f;
                if(travel && clipDuration>0 && stride>0) walk->timer=walk->start+float(std::fmod(a.distanceTravelledMeters/(stride*r.scale)*clipDuration,clipDuration));
                walk->last_update_time=walk->amount>0 ? walk->timer-1 : walk->timer;
            }
            if(idle) {
                idle->amount=travel?0.0f:1.0f;
                if(a.alive && !travel) idle->Play(); else { idle->Pause(); idle->last_update_time=idle->timer; }
            }
        }
    }
};
}
