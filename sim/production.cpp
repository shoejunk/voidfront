#include "voidfront_sim.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>

namespace vf {
uint32_t Sim::population_used(uint8_t player) const {
    if (player>=2) return 0;
    return static_cast<uint32_t>(std::count_if(units_.begin(),units_.end(),[&](const Unit& u) {
        return u.player==player && u.hp>0;
    }));
}
uint32_t Sim::population_reserved(uint8_t player) const {
    if (player>=2) return 0;
    uint32_t result=0;
    for (const auto& b:structures_) if (b.player==player && b.hp>0) result+=b.production_queue;
    return result;
}

void Sim::apply_production(const Command& c) {
    result_sequences_[c.player]=c.sequence;
    auto& result=results_[c.player];
    result=CommandResult::InvalidStructure;
    if (map_!=Map::Economy || c.units.size()!=1 || c.units.front()>structures_.size()) return;
    auto& b=structures_[c.units.front()-1];
    if (b.player!=c.player || b.hp<=0 || b.kind!=StructureKind::Foundry) return;
    if (b.build_ticks<kBuildTicks) { result=CommandResult::NotReady; return; }
    if (c.order==Order::CancelProduction) {
        if (!b.production_queue) { result=CommandResult::EmptyQueue; return; }
        // Cancel the tail. The active front retains its elapsed work unless it
        // was the only item, including when its completed exit is blocked.
        --b.production_queue;
        salvage_[c.player]+=kStriderCost;
        if (!b.production_queue) { b.production_ticks=0; b.spawn_blocked=false; }
    } else {
        if (b.production_queue>=kProductionQueueLimit) { result=CommandResult::QueueFull; return; }
        if (population_used(c.player)+population_reserved(c.player)>=kPopulationCap) { result=CommandResult::PopulationFull; return; }
        if (units_.size()+population_reserved(0)+population_reserved(1)>=kLifetimeUnitLimit) { result=CommandResult::RosterFull; return; }
        if (salvage_[c.player]<kStriderCost) { result=CommandResult::InsufficientSalvage; return; }
        salvage_[c.player]-=kStriderCost;
        ++b.production_queue;
    }
    result=CommandResult::Accepted;
}

void Sim::production_step() {
    if (map_!=Map::Economy) return;
    // Fixed perimeter exits, west first, then clockwise. Only these eight
    // explicitly clear points are exits; an obstructed factory waits in place.
    constexpr int exit_distance=kScale+64+32;
    constexpr std::array<nav::Point,8> offsets{{{-1,0},{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1}}};
    for (auto& b:structures_) {
        if (b.hp<=0 || b.kind!=StructureKind::Foundry || b.build_ticks<kBuildTicks || !b.production_queue) continue;
        if (b.production_ticks<kProductionTicks) ++b.production_ticks;
        if (b.production_ticks<kProductionTicks) continue;
        b.spawn_blocked=true;
        if (units_.size()>=kLifetimeUnitLimit) continue;
        for (const auto offset:offsets) {
            const nav::Point point{b.x+offset.x*exit_distance,b.z+offset.z*exit_distance};
            if (!navigation_.valid(point)) continue;
            const bool occupied=std::any_of(units_.begin(),units_.end(),[&](const Unit& u) {
                return u.hp>0 && std::abs(u.x-point.x)<128 && std::abs(u.z-point.z)<128;
            });
            if (occupied) continue;
            Unit u;
            // Dead entries remain as tombstones: IDs are never reused, and the
            // admission cap reserves lifetime space for all paid queue items.
            u.id=static_cast<uint32_t>(units_.size())+1; u.player=b.player;
            u.x=u.goal_x=u.next_x=point.x; u.z=u.goal_z=u.next_z=point.z;
            units_.push_back(u);
            --b.production_queue; b.production_ticks=0; b.spawn_blocked=false;
            break;
        }
    }
}
}
