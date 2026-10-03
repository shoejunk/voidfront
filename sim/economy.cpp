#include "voidfront_sim.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace vf {
namespace {
constexpr int radius=64;
nav::Point snapped(int32_t x,int32_t z) { return {x/kScale*kScale+kScale/2,z/kScale*kScale+kScale/2}; }
nav::Rect footprint(int32_t x,int32_t z,int32_t extent) { return {x-extent,z-extent,x+extent,z+extent}; }
bool overlaps(nav::Rect a,nav::Rect b) { return a.min_x<b.max_x && a.max_x>b.min_x && a.min_z<b.max_z && a.max_z>b.min_z; }
}

void Sim::set_goal(Unit& u,nav::Point goal) {
    u.goal_x=goal.x; u.goal_z=goal.z; u.next_x=u.x; u.next_z=u.z;
    u.path.clear(); u.detour.clear(); u.blocked_ticks=0; u.route_goal={-1,-1};
}

void Sim::rebuild_navigation() {
    auto obstacles=map_terrain(map_);
    for (const auto& b:structures_) if (b.hp>0) obstacles.push_back(footprint(b.x,b.z,kScale));
    for (const auto& d:deposits_) obstacles.push_back(footprint(d.x,d.z,kScale/2));
    navigation_=nav::World({kScale,kScale,(width()-1)*kScale,(height()-1)*kScale},std::move(obstacles),radius);
    for (auto& u:units_) {
        u.path.clear(); u.detour.clear(); u.blocked_ticks=0; u.route_goal={-1,-1}; u.next_x=u.x; u.next_z=u.z;
    }
}

nav::Point Sim::service_point(const Unit& u,int32_t x,int32_t z,int32_t extent,const nav::World* world) const {
    const auto& navigation=world?*world:navigation_;
    constexpr std::array<nav::Point,8> offsets{{{-1,0},{0,-1},{1,0},{0,1},{-1,-1},{1,-1},{1,1},{-1,1}}};
    for (size_t n=0;n<offsets.size();++n) {
        const auto d=offsets[(u.id-1+n)%offsets.size()];
        const nav::Point point{x+d.x*(extent+radius+32),z+d.z*(extent+radius+32)};
        if (!navigation.valid(point)) continue;
        bool free=true;
        for (const auto& other:units_) if (other.hp>0 && other.id!=u.id &&
            std::abs(other.x-point.x)<2*radius && std::abs(other.z-point.z)<2*radius) { free=false; break; }
        if (!free) continue;
        if (point==nav::Point{u.x,u.z} || !navigation.route({u.x,u.z},point).empty()) return point;
    }
    return {-1,-1};
}

bool Sim::can_build(uint8_t player,int32_t x,int32_t z) const {
    if (map_!=Map::Economy || player>=2 || x<0 || z<0 || x>=width()*kScale || z>=height()*kScale || structures_.size()>=30) return false;
    const auto center=snapped(x,z);
    const auto area=footprint(center.x,center.z,kScale);
    const auto buffer=footprint(center.x,center.z,2*kScale);
    if (buffer.min_x<kScale || buffer.min_z<kScale || buffer.max_x>(width()-1)*kScale || buffer.max_z>(height()-1)*kScale) return false;
    bool linked=false;
    for (const auto& b:structures_) if (b.hp>0) {
        if (overlaps(buffer,footprint(b.x,b.z,kScale))) return false;
        const int64_t dx=int64_t(b.x)-center.x,dz=int64_t(b.z)-center.z;
        if (b.player==player && b.kind==StructureKind::Anchor && dx*dx+dz*dz<=int64_t(8*kScale)*(8*kScale)) linked=true;
    }
    if (!linked) return false;
    for (const auto& r:map_terrain(map_)) if (overlaps(buffer,r)) return false;
    for (const auto& d:deposits_) if (overlaps(buffer,footprint(d.x,d.z,kScale/2))) return false;
    for (const auto& u:units_) if (u.hp>0 && overlaps(area,footprint(u.x,u.z,radius))) return false;
    return true;
}

void Sim::apply_economy(const Command& c) {
    if (c.order==Order::TrainStrider || c.order==Order::TrainLancer || c.order==Order::CancelProduction || c.order==Order::Research) { apply_production(c); return; }
    result_sequences_[c.player]=c.sequence;
    results_[c.player]=CommandResult::InvalidWorker;
    if (map_!=Map::Economy) return;
    std::vector<uint32_t> workers;
    for (auto id:c.units) if (units_[id-1].hp>0 && units_[id-1].kind==UnitKind::Worker) workers.push_back(id);
    if (workers.empty()) return;
    if (c.order==Order::Build) {
        const auto center=snapped(c.x,c.z);
        uint32_t building=0;
        for (const auto& b:structures_) if (b.player==c.player && b.hp>0 && b.kind==StructureKind::Foundry &&
            b.build_ticks<kBuildTicks && b.x==center.x && b.z==center.z) building=b.id;
        auto& worker=units_[workers.front()-1];
        if (!building) {
            if (!can_build(c.player,c.x,c.z)) { results_[c.player]=CommandResult::InvalidPlacement; return; }
            if (salvage_[c.player]<kFoundryCost) { results_[c.player]=CommandResult::InsufficientSalvage; return; }
            // Bounds, spacing and entity cap are validated before any debit.
            // Navigation is deterministic and construction cannot overlap a worker.
            auto obstacles=map_terrain(map_);
            for (const auto& b:structures_) if (b.hp>0) obstacles.push_back(footprint(b.x,b.z,kScale));
            for (const auto& d:deposits_) obstacles.push_back(footprint(d.x,d.z,kScale/2));
            obstacles.push_back(footprint(center.x,center.z,kScale));
            const nav::World proposed({kScale,kScale,(width()-1)*kScale,(height()-1)*kScale},std::move(obstacles),radius);
            const auto approach=service_point(worker,center.x,center.z,kScale,&proposed);
            if (approach.x<0) { results_[c.player]=CommandResult::InvalidPlacement; return; }
            building=static_cast<uint32_t>(structures_.size())+1;
            structures_.push_back({building,c.player,StructureKind::Foundry,center.x,center.z,500,0});
            salvage_[c.player]-=kFoundryCost;
            rebuild_navigation();
        }
        const auto approach=service_point(worker,center.x,center.z,kScale);
        if (approach.x<0) { results_[c.player]=CommandResult::InvalidPlacement; return; }
        worker.queue.clear(); worker.order=Order::Build; worker.build_id=building; worker.resource_id=0; worker.returning=false; worker.work_ticks=0;
        set_goal(worker,approach);
        results_[c.player]=CommandResult::Accepted;
        return;
    }
    uint32_t deposit=0;
    if (c.order==Order::Gather) {
        for (const auto& d:deposits_) if (std::abs(d.x-c.x)<=kScale/2 && std::abs(d.z-c.z)<=kScale/2 && d.remaining>0) { deposit=d.id; break; }
        if (!deposit) { results_[c.player]=CommandResult::InvalidTarget; return; }
    }
    for (auto id:workers) {
        auto& u=units_[id-1];
        u.queue.clear(); u.order=c.order; u.resource_id=deposit; u.build_id=0; u.work_ticks=0;
        // Mixed cargo is delivered first; resource_id then resumes the new deposit.
        const bool mixed=deposit && u.cargo>0 && deposits_[deposit-1].kind!=u.cargo_kind;
        u.returning=c.order==Order::ReturnCargo || u.cargo>=kCargoCapacity || mixed;
        set_goal(u,{u.x,u.z});
    }
    results_[c.player]=CommandResult::Accepted;
}

void Sim::economy_step() {
    if (map_!=Map::Economy) return;
    // Stable worker ordering resolves simultaneous last-unit extraction.
    std::vector<bool> worked(structures_.size(),false);
    const auto occupied_goal=[&](const Unit& u) {
        if (tick_%20!=u.id%20) return false;
        for (const auto& other:units_) if (other.hp>0 && other.id!=u.id &&
            std::abs(other.x-u.goal_x)<2*radius && std::abs(other.z-u.goal_z)<2*radius) return true;
        return false;
    };
    for (auto& u:units_) {
        if (u.hp<=0 || u.kind!=UnitKind::Worker) continue;
        if (u.order==Order::Build) {
            if (!u.build_id || u.build_id>structures_.size()) { u.order=Order::Stop; continue; }
            auto& b=structures_[u.build_id-1];
            if (b.hp<=0 || b.player!=u.player || b.build_ticks>=kBuildTicks) { u.order=Order::Stop; continue; }
            if (occupied_goal(u)) {
                const auto goal=service_point(u,b.x,b.z,kScale);
                if (goal.x>=0 && (goal.x!=u.goal_x || goal.z!=u.goal_z)) set_goal(u,goal);
            }
            if (u.x==u.goal_x && u.z==u.goal_z && !worked[b.id-1]) { ++b.build_ticks; worked[b.id-1]=true; }
            if (b.build_ticks>=kBuildTicks) u.order=Order::Stop;
            continue;
        }
        if (u.order!=Order::Gather && u.order!=Order::ReturnCargo) continue;
        Deposit* d=u.resource_id && u.resource_id<=deposits_.size()?&deposits_[u.resource_id-1]:nullptr;
        if (!u.returning && (!d || d->remaining==0)) {
            if (u.cargo>0) { u.returning=true; u.work_ticks=0; set_goal(u,{u.x,u.z}); }
            else { u.order=Order::Stop; continue; }
        }
        int32_t target_x=0,target_z=0,extent=0;
        if (u.returning) {
            const Structure* anchor=nullptr;
            for (const auto& b:structures_) if (b.player==u.player && b.hp>0 && b.kind==StructureKind::Anchor) { anchor=&b; break; }
            if (!anchor) { u.order=Order::Stop; continue; }
            target_x=anchor->x; target_z=anchor->z; extent=kScale;
        } else { target_x=d->x; target_z=d->z; extent=kScale/2; }
        // An unset service destination is initialized on state transitions only.
        // Nearby workers receive different deterministic perimeter slots.
        const int32_t distance=std::max(std::abs(u.goal_x-target_x),std::abs(u.goal_z-target_z));
        if (distance!=extent+radius+32 || occupied_goal(u)) {
            const auto goal=service_point(u,target_x,target_z,extent);
            if (goal.x<0) continue;
            set_goal(u,goal);
        }
        if (u.x!=u.goal_x || u.z!=u.goal_z) continue;
        if (u.returning) {
            (u.cargo_kind?flux_:salvage_)[u.player]+=u.cargo; u.cargo=0; u.returning=false; u.work_ticks=0;
            if (u.order==Order::ReturnCargo || !d || d->remaining==0) u.order=Order::Stop;
            else set_goal(u,{u.x,u.z});
        } else if (++u.work_ticks>=kGatherTicks) {
            u.work_ticks=0;
            if (d->remaining>0) { --d->remaining; ++u.cargo; u.cargo_kind=d->kind; }
            if (u.cargo>=kCargoCapacity || d->remaining==0) { u.returning=true; set_goal(u,{u.x,u.z}); }
        }
    }
}
}
