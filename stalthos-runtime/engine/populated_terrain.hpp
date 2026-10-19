#pragma once
#include "WickedEngine.h"
#include "json.hpp"
#include "domain/watershed_terrain.hpp"
#include "domain/connected_route.hpp"
#include "organic_rocks.hpp"
#include "meadow_scene.hpp"
#include "terrain_surface.hpp"
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
    world::ConnectedRoute route_;
    bool connected_;
    domain::ReferenceLandscape reference_;
    std::size_t routeTufts_=0,routeRocks_=0;
    std::vector<wi::ecs::Entity> routeScenery_;
    bool dressed_=false;
    world::TerrainVertex sample(double x,double z) const {return connected_?route_.sample(x,z):source_.sample(x,z);}
    void dressRoute(wi::scene::Scene& scene) {
        if(dressed_ || !connected_) return; dressed_=true;
        // A bounded composition belongs to this route, not camera activation.
        const auto grass=scene.Entity_CreateMaterial("sandy lowland dry grasses");
        auto* material=scene.materials.GetComponent(grass);material->SetBaseColor({.67f,.63f,.38f,1});material->SetRoughness(.94f);material->SetDoubleSided(true);material->SetUseVertexColors(true);
        meadow_detail::MeadowBatchGeometry batch;
        for(int ix=0;ix<60;++ix) for(int iz=0;iz<30;++iz) {
            const auto key=std::uint64_t(ix*7919+iz*104729+42);
            const double gx=route_.layout().joinX+4+ix*((route_.layout().meadowX-route_.layout().joinX+40)/60)+domain::hashUnit(domain::hashMix(key))*1.7;
            const double gz=route_.layout().centerZ-40+iz*2.7+domain::hashUnit(domain::hashMix(key+1))*1.9;
            // Leave a broad readable dry trail; clusters widen into the meadow.
            if(std::abs(gz-route_.layout().centerZ)<2.8 || domain::hashUnit(domain::hashMix(key+2))<.32) continue;
            meadow_detail::appendTuft(batch,gx,route_.triangleHeight(gx,gz),gz,domain::Habitat::OpenLowland,key);++routeTufts_;
        }
        routeScenery_.push_back(meadow_detail::createMeadowBatchEntity(scene,"east lowland grass mosaic",std::move(batch),grass));
        const auto rockMaterial=scene.Entity_CreateMaterial("weathered sandstone route landmarks");
        scene.materials.GetComponent(rockMaterial)->SetBaseColor({.47f,.40f,.29f,1});scene.materials.GetComponent(rockMaterial)->SetRoughness(.97f);
        // Weathered rock clusters frame the saddle and two meadow edges. Natural
        // faceted meshes reuse the established organic-rock generator, never boxes.
        const auto& layout=route_.layout();
        const std::array<std::pair<double,double>,5> clusters{{{layout.joinX+16,layout.centerZ-16},{layout.joinX+30,layout.centerZ+20},{layout.meadowX-24,layout.centerZ-28},{layout.meadowX+4,layout.centerZ+29},{layout.meadowX+24,layout.centerZ-18}}};
        for(std::size_t c=0;c<clusters.size();++c) for(int n=0;n<5;++n) {
            const auto key=std::uint64_t(c*401+n*31+123);
            const double gx=clusters[c].first+std::sin(double(n)*2.4)*4,gz=clusters[c].second+std::cos(double(n)*2.4)*3;
            auto geometry=detail::createOrganicRockMesh(domain::PlacementKind::Boulder,n==0?3.1:1.2+double(n)*.15,key);
            const auto entity=scene.Entity_CreateMeshFromData("weathered east route outcrop",geometry.indices.size(),geometry.indices.data(),geometry.positions.size(),geometry.positions.data());
            const auto* object=scene.objects.GetComponent(entity);auto* mesh=scene.meshes.GetComponent(object->meshID);for(auto& subset:mesh->subsets) subset.materialID=rockMaterial;
            auto* transform=scene.transforms.GetComponent(entity);transform->Translate(DirectX::XMFLOAT3{float(gx),float(route_.triangleHeight(gx,gz)+geometry.groundOffset),float(gz)});transform->UpdateTransform();mesh->CreateRenderData();
            routeScenery_.push_back(entity);++routeRocks_;
        }
    }
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
    explicit PopulatedTerrain(std::uint64_t seed,wi::scene::Scene& scene,bool connected=false):source_(seed,{64,2.0,1.0}),route_(seed),connected_(connected),reference_(seed) {
        const auto entity=scene.Entity_FindByName("draft reference ground");
        const auto* object=scene.objects.GetComponent(entity);
        const auto* mesh=object?scene.meshes.GetComponent(object->meshID):nullptr;
        if(!mesh || !local_.build(mesh->vertex_positions.data(),mesh->vertex_positions.size(),mesh->indices.data(),mesh->indices.size())) throw std::runtime_error("populated terrain missing reference ground");
        for(std::size_t i=0;i<mesh->vertex_positions.size();++i) {
            const auto& p=mesh->vertex_positions[i];
            if(std::abs(p.x)==96 || std::abs(p.z)==96) boundary_[{int(p.x),int(p.z)}]=mesh->vertex_normals.at(i);
        }
    }
    double analytical(double x,double z) const {return connected_?route_.height(x,z):source_.height(x,z);}
    double height(double x,double z) const {
        if(local_.inBounds(x,z)) return local_.sampleHeight(x,z);
        // Include adjacent resident pieces at exact shared edges.
        for(const auto& tile:resident_) for(const auto& patch:tile.second)
            if(patch.ground.inBounds(x,z)) return patch.ground.sampleHeight(x,z);
        throw std::out_of_range("populated rendered ground is not resident");
    }
    void update(wi::scene::Scene& scene,double x,double z) {
        dressRoute(scene);
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
                    const auto v=sample(vx,vz);
                    positions.push_back({float(vx),float(v.position.y),float(vz)});
                    auto n=DirectX::XMFLOAT3(float(v.normal.x),float(v.normal.y),float(v.normal.z));
                    const auto old=boundary_.find({vx,vz});if(old!=boundary_.end()) n=old->second;
                    normals.push_back(n);
                }
                for(int zz=0;zz<nz-1;++zz) for(int xx=0;xx<nx-1;++xx) {const std::uint32_t a=zz*nx+xx,b=a+1,c=a+nx,d=c+1;indices.insert(indices.end(),{a,b,c,b,d,c});}
                const auto entity=scene.Entity_CreateMeshFromData("populated exterior terrain",indices.size(),indices.data(),positions.size(),positions.data());
                const auto* object=scene.objects.GetComponent(entity);auto* mesh=scene.meshes.GetComponent(object->meshID);
                mesh->vertex_normals=normals;
                for(const auto& subset:mesh->subsets) {auto* material=scene.materials.GetComponent(subset.materialID);material->SetBaseColor(connected_?DirectX::XMFLOAT4{1,1,1,1}:DirectX::XMFLOAT4{.32f,.38f,.22f,1});material->SetRoughness(.96f);}
                if(connected_) {
                    for(const auto& vertex:positions) {
                        using Habitat=domain::Habitat;
                        const auto habitat=reference_.habitat(std::clamp(double(vertex.x),-96.0,96.0),std::clamp(double(vertex.z),-96.0,96.0));
                        wi::Color green(83,97,56);
                        if(habitat==Habitat::Creek) green=wi::Color(91,81,63);else if(habitat==Habitat::Riparian) green=wi::Color(72,93,48);else if(habitat==Habitat::Woodland) green=wi::Color(61,80,42);else if(habitat==Habitat::Bluff || habitat==Habitat::Outcrop) green=wi::Color(112,104,89);
                        const float t=std::clamp((vertex.x-96.f)/56.f,0.f,1.f);
                        const float mosaic=1.f-t*.04f+t*.04f*std::sin(vertex.x*.12f)*std::sin(vertex.z*.19f);
                        mesh->vertex_colors.push_back(wi::Color(uint8_t((green.getR()*(1-t)+131*t)*mosaic),uint8_t((green.getG()*(1-t)+122*t)*mosaic),uint8_t((green.getB()*(1-t)+76*t)*mosaic)).rgba);
                    }
                    for(const auto& subset:mesh->subsets) scene.materials.GetComponent(subset.materialID)->SetUseVertexColors(true);
                    applyTerrainSurface(scene,entity,reference_,42);
                }
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
    nlohmann::json receipt() const {return {{"connected_world_preview",connected_},{"route_grass_tufts",routeTufts_},{"route_organic_rocks",routeRocks_},{"created_tiles",created_},{"evicted_tiles",evicted_},{"regenerated_tiles",regenerated_},{"regenerated_mesh_mismatches",mismatches_},{"peak_resident_tiles",peak_},{"resident_tiles",resident_.size()},{"body_tile_crossings",crossings_},{"visited_body_tiles",visited_.size()},{"shared_edge_samples",seamChecks_},{"max_shared_height_error_m",seamHeight_},{"max_shared_normal_component_error",seamNormal_},{"reference_mesh_retained",true},{"exterior_cells_overlap_reference",false},{"residency_driver","body"},{"population_activation_implemented",false}};}
};
}
