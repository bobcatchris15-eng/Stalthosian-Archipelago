#pragma once
#include "WickedEngine.h"
#include "json.hpp"
#include "domain/watershed_terrain.hpp"
#include "domain/rendered_ground_surface.hpp"
#include <map>
#include <set>
#include <cstring>
#include <algorithm>
#include <cstdint>

namespace stalthos::engine {
// Retain the inhabited mesh verbatim. Exterior tiles are rectangular pieces,
// split at the existing boundary so their actual indices never overlap it.
class PopulatedTerrain {
    using Key=std::pair<std::int64_t,std::int64_t>;
    struct Patch { wi::ecs::Entity entity; domain::RenderedGroundSurface ground; std::vector<DirectX::XMFLOAT3> normals; };
    world::WatershedTerrain source_;
    domain::RenderedGroundSurface local_;
    std::map<std::pair<int,int>,DirectX::XMFLOAT3> boundary_;
    std::map<Key,std::vector<Patch>> resident_;
    std::map<Key,std::uint64_t> digests_;
    std::set<Key> visited_;
    std::size_t created_=0,evicted_=0,regenerated_=0,mismatches_=0,peak_=0,crossings_=0;
    std::uint64_t seamChecks_=0;
    double seamHeight_=0,seamNormal_=0;
    Key previous_{INT64_MAX,INT64_MAX};
public:
    explicit PopulatedTerrain(std::uint64_t seed,wi::scene::Scene& scene):source_(seed,{64,2.0,1.0}) {
        const auto entity=scene.Entity_FindByName("draft reference ground");
        const auto* object=scene.objects.GetComponent(entity);
        const auto* mesh=object?scene.meshes.GetComponent(object->meshID):nullptr;
        if(!mesh || !local_.build(mesh->vertex_positions.data(),mesh->vertex_positions.size(),mesh->indices.data(),mesh->indices.size())) throw std::runtime_error("populated terrain missing reference ground");
        for(std::size_t i=0;i<mesh->vertex_positions.size();++i) {
            const auto& p=mesh->vertex_positions[i];
            if(std::abs(p.x)==96 || std::abs(p.z)==96) boundary_[{int(p.x),int(p.z)}]=mesh->vertex_normals.at(i);
        }
    }
    double analytical(double x,double z) const {return source_.height(x,z);}
    double height(double x,double z) const {
        if(local_.inBounds(x,z)) return local_.sampleHeight(x,z);
        // Include adjacent resident pieces at exact shared edges.
        for(const auto& tile:resident_) for(const auto& patch:tile.second)
            if(patch.ground.inBounds(x,z)) return patch.ground.sampleHeight(x,z);
        throw std::out_of_range("populated rendered ground is not resident");
    }
    void update(wi::scene::Scene& scene,double x,double z) {
        const auto body=source_.bodyTile({x,0,z}); const Key key{body.x,body.z};
        if(key==previous_) return;
        if(previous_.first!=INT64_MAX) ++crossings_;
        previous_=key; visited_.insert(key);
        std::set<Key> wanted;
        for(const auto k:source_.neighborhood({x,0,z})) wanted.emplace(k.x,k.z);
        for(int iz=-1;iz<=0;++iz) for(int ix=-1;ix<=0;++ix) wanted.emplace(ix,iz);
        for(auto it=resident_.begin();it!=resident_.end();) if(!wanted.count(it->first)) {
            for(const auto& p:it->second) scene.Entity_Remove(p.entity);
            it=resident_.erase(it); ++evicted_;
        } else ++it;
        for(const auto k:wanted) if(!resident_.count(k)) {
            std::vector<Patch> patches;
            std::uint64_t digest=1469598103934665603ULL;
            const auto hash=[&](float value) {std::uint32_t bits; std::memcpy(&bits,&value,4); digest^=bits;digest*=1099511628211ULL;};
            const int minX=int(k.first*128),minZ=int(k.second*128);
            std::vector<int> xs{minX,minX+128},zs{minZ,minZ+128};
            for(int edge:{-96,96}) {if(edge>minX && edge<minX+128) xs.push_back(edge);if(edge>minZ && edge<minZ+128) zs.push_back(edge);}
            std::sort(xs.begin(),xs.end());std::sort(zs.begin(),zs.end());
            for(std::size_t iz=1;iz<zs.size();++iz) for(std::size_t ix=1;ix<xs.size();++ix) {
                const int left=xs[ix-1],right=xs[ix],back=zs[iz-1],front=zs[iz];
                if(left>=-96 && right<=96 && back>=-96 && front<=96) continue;
                std::vector<DirectX::XMFLOAT3> positions,normals;std::vector<std::uint32_t> indices;
                const int nx=(right-left)/2+1,nz=(front-back)/2+1;
                for(int vz=back;vz<=front;vz+=2) for(int vx=left;vx<=right;vx+=2) {
                    const auto v=source_.sample(vx,vz);
                    positions.push_back({float(vx),float(v.position.y),float(vz)});
                    auto n=DirectX::XMFLOAT3(float(v.normal.x),float(v.normal.y),float(v.normal.z));
                    const auto old=boundary_.find({vx,vz});if(old!=boundary_.end()) n=old->second;
                    normals.push_back(n);
                }
                for(int zz=0;zz<nz-1;++zz) for(int xx=0;xx<nx-1;++xx) {const std::uint32_t a=zz*nx+xx,b=a+1,c=a+nx,d=c+1;indices.insert(indices.end(),{a,b,c,b,d,c});}
                const auto entity=scene.Entity_CreateMeshFromData("populated exterior terrain",indices.size(),indices.data(),positions.size(),positions.data());
                const auto* object=scene.objects.GetComponent(entity);auto* mesh=scene.meshes.GetComponent(object->meshID);
                mesh->vertex_normals=normals;
                for(const auto& subset:mesh->subsets) {auto* material=scene.materials.GetComponent(subset.materialID);material->SetBaseColor({.32f,.38f,.22f,1});material->SetRoughness(.96f);}
                mesh->CreateRenderData();
                Patch patch{entity,{},mesh->vertex_normals};
                if(!patch.ground.build(mesh->vertex_positions.data(),mesh->vertex_positions.size(),mesh->indices.data(),mesh->indices.size())) throw std::runtime_error(patch.ground.errorMessage());
                for(std::size_t i=0;i<mesh->vertex_positions.size();++i) {const auto& p=mesh->vertex_positions[i];const auto& n=mesh->vertex_normals[i];hash(p.x);hash(p.y);hash(p.z);hash(n.x);hash(n.y);hash(n.z);}
                patches.push_back(std::move(patch));
            }
            if(digests_.count(k)) {++regenerated_;if(digests_.at(k)!=digest) ++mismatches_;} else digests_[k]=digest;
            resident_.emplace(k,std::move(patches));++created_;
        }
        peak_=std::max(peak_,resident_.size());
        std::map<std::pair<int,int>,std::pair<double,DirectX::XMFLOAT3>> edges;
        for(const auto& p:local_.worldVertices()) {const auto n=boundary_.find({int(p.x),int(p.z)});if(n!=boundary_.end()) edges[{int(p.x),int(p.z)}]={p.y,n->second};}
        for(const auto& tile:resident_) for(const auto& patch:tile.second) {
            const auto& vertices=patch.ground.worldVertices();
            for(std::size_t i=0;i<vertices.size();++i) {
                const auto& p=vertices[i];const auto& n=patch.normals[i];const auto key=std::make_pair(int(p.x),int(p.z));
                const auto old=edges.find(key);
                if(old!=edges.end()) {++seamChecks_;seamHeight_=std::max(seamHeight_,std::abs(p.y-old->second.first));const auto m=old->second.second;seamNormal_=std::max(seamNormal_,double(std::max({std::abs(n.x-m.x),std::abs(n.y-m.y),std::abs(n.z-m.z)})));}
                else edges[key]={p.y,n};
            }
        }
    }
    nlohmann::json receipt() const {return {{"created_tiles",created_},{"evicted_tiles",evicted_},{"regenerated_tiles",regenerated_},{"regenerated_mesh_mismatches",mismatches_},{"peak_resident_tiles",peak_},{"resident_tiles",resident_.size()},{"body_tile_crossings",crossings_},{"visited_body_tiles",visited_.size()},{"shared_edge_samples",seamChecks_},{"max_shared_height_error_m",seamHeight_},{"max_shared_normal_component_error",seamNormal_},{"reference_mesh_retained",true},{"exterior_cells_overlap_reference",false},{"residency_driver","body"},{"population_activation_implemented",false}};}
};
}
