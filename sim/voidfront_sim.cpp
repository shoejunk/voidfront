#include "voidfront_sim.hpp"
#include "spatial.hpp"
#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace vf {
int map_width(Map map) {
    if ((map==Map::Foundry || map==Map::Economy)) return kMapWidth;
    if (map==Map::Scale128) return 128;
    throw std::invalid_argument("unsupported map");
}
int map_height(Map map) { return (map==Map::Foundry || map==Map::Economy)?kMapHeight:map_width(map); }
const std::vector<nav::Rect>& map_terrain(Map map) {
    static const std::vector<nav::Rect> foundry{{15*kScale,3*kScale,17*kScale,9*kScale},
        {15*kScale,16*kScale,17*kScale,21*kScale}};
    static const std::vector<nav::Rect> scale{{63*kScale,8*kScale,65*kScale,60*kScale},
        {63*kScale,68*kScale,65*kScale,120*kScale}};
    if ((map==Map::Foundry || map==Map::Economy)) return foundry;
    if (map==Map::Scale128) return scale;
    throw std::invalid_argument("unsupported map");
}
namespace {
nav::Rect terrain_bounds(Map map) { return {kScale,kScale,(map_width(map)-1)*kScale,(map_height(map)-1)*kScale}; }
constexpr int unit_radius = 64, movement_speed = 32;
int64_t distance2(const Unit& a, const Unit& b) {
    const int64_t dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}
bool canonical(const Command& c) {
    return c.player < 2 && c.sequence > 0 && static_cast<uint8_t>(c.order) <= 8 &&
        (static_cast<uint8_t>(c.order)<7 || (c.units.size()==1 && c.x==0 && c.z==0)) &&
        !c.units.empty() && c.units.size() <= 256 && c.x >= 0 && c.z >= 0 &&
        c.x < kMaxMapSize * kScale && c.z < kMaxMapSize * kScale &&
        std::is_sorted(c.units.begin(), c.units.end()) &&
        std::adjacent_find(c.units.begin(), c.units.end()) == c.units.end() && c.units.front() > 0;
}
bool command_less(const Command& a, const Command& b) {
    return std::tie(a.tick, a.player, a.sequence) < std::tie(b.tick, b.player, b.sequence);
}
void append(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}
void step_candidates(nav::Point here,nav::Point waypoint,std::vector<nav::Point>& result) {
    result.clear();
    const int64_t dx=int64_t(waypoint.x)-here.x,dz=int64_t(waypoint.z)-here.z;
    const auto length=nav::ceil_sqrt(static_cast<uint64_t>(dx*dx+dz*dz));
    if (length<=movement_speed) { result.push_back(waypoint); return; }
    const int32_t sx=static_cast<int32_t>(dx*movement_speed/static_cast<int64_t>(length));
    const int32_t sz=static_cast<int32_t>(dz*movement_speed/static_cast<int64_t>(length));
    for (int x=-1;x<=1;++x) for (int z=-1;z<=1;++z) {
        const int64_t nx=sx+x,nz=sz+z;
        if (nx*nx+nz*nz<=movement_speed*movement_speed &&
            (dx-nx)*(dx-nx)+(dz-nz)*(dz-nz)<dx*dx+dz*dz)
            result.push_back({here.x+static_cast<int32_t>(nx),here.z+static_cast<int32_t>(nz)});
    }
    const auto error=[&](nav::Point p) {
        const int64_t ex=(int64_t(p.x)-here.x)*static_cast<int64_t>(length)-dx*movement_speed;
        const int64_t ez=(int64_t(p.z)-here.z)*static_cast<int64_t>(length)-dz*movement_speed;
        return ex*ex+ez*ez;
    };
    std::sort(result.begin(),result.end(),[&](nav::Point a,nav::Point b) {
        const auto ea=error(a),eb=error(b);
        return ea!=eb?ea<eb:std::tie(a.x,a.z)<std::tie(b.x,b.z);
    });
}
}

Sim::Sim(uint32_t seed, uint32_t count, Map map) : map_(map), rng_(seed ? seed : 1), navigation_(terrain_bounds(map), map_terrain(map), unit_radius) {
    if (count < 1 || count > 250) throw std::invalid_argument("units_per_team must be 1..250");
    if (map==Map::Economy) {
        for (uint8_t p=0;p<2;++p) {
            structures_.push_back({uint32_t(p)+1,p,StructureKind::Anchor,(p==0?4:27)*kScale+128,12*kScale+128});
            deposits_.push_back({uint32_t(p)+1,(p==0?7:24)*kScale+128,7*kScale+128,2000});
            for (int n=0;n<3;++n) {
                Unit u;
                u.id=static_cast<uint32_t>(units_.size())+1; u.player=p; u.kind=UnitKind::Worker;
                u.x=u.goal_x=u.next_x=(p==0?6:25)*kScale+128;
                u.z=u.goal_z=u.next_z=(11+n)*kScale+128;
                units_.push_back(u);
            }
        }
        rebuild_navigation();
        return;
    }
    for (uint8_t p = 0; p < 2; ++p) {
        for (uint32_t n = 0; n < count; ++n) {
            Unit u;
            u.id = static_cast<uint32_t>(units_.size()) + 1;
            u.player = p;
            const int x = map==Map::Foundry ? (p == 0 ? 2 + static_cast<int>(n / 20) : 29 - static_cast<int>(n / 20)) :
                (p == 0 ? 16 + static_cast<int>(n / 20) : 111 - static_cast<int>(n / 20));
            const int z = (count <= 12 ? 9 + static_cast<int>(n) : 2 + static_cast<int>(n % 20)) +
                (map==Map::Scale128?52:0);
            u.x = u.goal_x = u.next_x = x * kScale + kScale / 2;
            u.z = u.goal_z = u.next_z = z * kScale + kScale / 2;
            rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
            u.cooldown = static_cast<uint16_t>(rng_ % 5);
            units_.push_back(u);
        }
    }
}

bool Sim::blocked(int x, int z) const {
    if (x <= 0 || z <= 0 || x >= width() - 1 || z >= height() - 1) return true;
    if (map_==Map::Economy) return !navigation_.valid({x*kScale+kScale/2,z*kScale+kScale/2});
    for (const auto& r:map_terrain(map_))
        if (x*kScale>=r.min_x && x*kScale<r.max_x && z*kScale>=r.min_z && z*kScale<r.max_z) return true;
    for (const auto& b:structures_) if (b.hp>0 && x*kScale+kScale/2>=b.x-kScale && x*kScale+kScale/2<b.x+kScale &&
        z*kScale+kScale/2>=b.z-kScale && z*kScale+kScale/2<b.z+kScale) return true;
    for (const auto& d:deposits_) if (x==d.x/kScale && z==d.z/kScale) return true;
    return false;
}

bool Sim::submit(Command command) {
    std::sort(command.units.begin(), command.units.end());
    if (!canonical(command) || command.x>=width()*kScale || command.z>=height()*kScale ||
        command.tick < tick_ || command.tick - tick_ > 1200 ||
        command.sequence <= last_sequence_[command.player] || pending_.size() >= 4096) return false;
    for (const auto& c : pending_)
        if (c.player == command.player && c.sequence == command.sequence) return false;
    // A player's sequence order must agree with tick order even if packets arrive reordered.
    for (const auto& c : pending_)
        if (c.player == command.player && ((c.sequence < command.sequence && c.tick > command.tick) ||
            (c.sequence > command.sequence && c.tick < command.tick))) return false;
    if (command.order==Order::TrainStrider || command.order==Order::CancelProduction) {
        const auto id=command.units.front();
        if (map_!=Map::Economy || id>structures_.size() || structures_[id-1].player!=command.player) return false;
    } else for (const auto id : command.units)
        if (id > units_.size() || units_[id - 1].player != command.player) return false;
    pending_.push_back(std::move(command));
    std::sort(pending_.begin(), pending_.end(), command_less);
    return true;
}

void Sim::apply(const Command& c) {
    last_sequence_[c.player] = c.sequence;
    // Already-buffered lockstep inputs still consume their sequence after the
    // result, but can no longer change the economy or battlefield.
    if (map_==Map::Economy && winner()!=-1) return;
    if (static_cast<uint8_t>(c.order)>=4) { apply_economy(c); return; }
    if (map_==Map::Economy) { results_[c.player]=CommandResult::Accepted; result_sequences_[c.player]=c.sequence; }
    // Enumerate Manhattan rings once in row-major order. This preserves the
    // original exhaustive search's exact distance/row/column ties without
    // rescanning every map cell for each selected unit.
    std::vector<nav::Point> slots;
    if (c.order==Order::Move || c.order==Order::AttackMove) {
        const int rx=c.x/kScale,rz=c.z/kScale;
        for (int radius=0;radius<width()+height() && slots.size()<c.units.size();++radius) {
            for (int z=std::max(0,rz-radius);z<=std::min(height()-1,rz+radius) && slots.size()<c.units.size();++z) {
                const int dx=radius-std::abs(z-rz);
                for (const int x : {rx-dx,rx+dx}) {
                    if (!blocked(x,z)) slots.push_back({x*kScale+kScale/2,z*kScale+kScale/2});
                    if (dx==0 || slots.size()==c.units.size()) break;
                }
            }
        }
    }
    size_t next_slot=0;
    for (const auto id : c.units) {
        auto& u = units_[id - 1];
        if (u.hp <= 0) continue;
        u.resource_id=0; u.build_id=0; u.work_ticks=0; u.returning=false;
        u.order = c.order;
        u.target_id = 0;
        u.target_structure_id = 0;
        u.path.clear(); u.next_x = u.x; u.next_z = u.z;
        u.detour.clear(); u.blocked_ticks=0;
        u.route_goal = {-1,-1};
        if (c.order == Order::Stop || c.order == Order::Hold) {
            u.goal_x = u.x; u.goal_z = u.z;
            continue;
        }
        // Single-unit orders retain the exact fixed-point destination when feasible.
        if (c.units.size() == 1 && navigation_.valid({c.x,c.z})) {
            u.goal_x = c.x; u.goal_z = c.z;
            continue;
        }
        if (next_slot<slots.size()) {
            u.goal_x = slots[next_slot].x; u.goal_z = slots[next_slot++].z;
        }
    }
}

void Sim::step() {
    size_t applied = 0;
    while (applied < pending_.size() && pending_[applied].tick == tick_) apply(pending_[applied++]);
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(applied));
    if (map_==Map::Economy && winner()!=-1) { ++tick_; return; }
    economy_step();
    production_step();
    std::vector<int32_t> damage(units_.size(), 0);
    std::vector<int32_t> structure_damage(structures_.size(),0);
    std::vector<nav::Point> tick_start;
    tick_start.reserve(units_.size());
    SpatialIndex spatial(width(),height(),kScale);
    for (const auto& u : units_) {
        tick_start.push_back({u.x,u.z});
        if (u.hp>0) spatial.insert(u.id,{u.x,u.z});
    }
    std::vector<uint32_t> nearby;
    std::vector<uint32_t> local;
    std::vector<nav::Point> candidates;
    candidates.reserve(9);
    std::vector<std::vector<nav::Point>> attempted_detours(units_.size());
    std::vector<uint8_t> reconsidered_detour(units_.size(),0);
    constexpr int64_t attack_range2 = int64_t(3 * kScale) * (3 * kScale);
    constexpr int64_t acquire_range2 = int64_t(6 * kScale) * (6 * kScale);
    for (auto& u : units_) {
        u.moving = false; u.target_id = 0; u.target_structure_id=0;
        if (u.hp <= 0) continue;
        if (u.cooldown > 0) --u.cooldown;
        const Unit* target = nullptr;
        int64_t nearest = acquire_range2 + 1;
        if (u.kind==UnitKind::Strider && u.order != Order::Move) {
            constexpr int reach=6*kScale+movement_speed;
            spatial.query({u.x-reach,u.z-reach,u.x+reach,u.z+reach},nearby);
            for (const auto id : nearby) {
                const auto& other=units_[id-1];
                if (other.hp <= 0 || other.player == u.player) continue;
                const auto d = distance2(u, other);
                if (d < nearest) { nearest = d; target = &other; }
            }
        }
        nav::Point goal{u.goal_x, u.goal_z};
        if (target) {
            u.target_id = target->id;
            if (nearest <= attack_range2) {
                if (u.cooldown == 0) { damage[target->id - 1] += 8; u.cooldown = 10; }
                goal = {u.x, u.z};
            } else if (u.order == Order::AttackMove) goal = {target->x, target->z};
        }
        // Units have priority; otherwise acquire the nearest enemy building by
        // squared distance to its footprint, resolving ties by stable ID.
        // Structures have a separate ID space, exposed separately in snapshots.
        if (!target && u.kind==UnitKind::Strider && u.order!=Order::Move) {
            const Structure* building=nullptr;
            int64_t distance=acquire_range2+1;
            for (const auto& b:structures_) if (b.hp>0 && b.player!=u.player) {
                const int64_t dx=std::max(0,std::abs(u.x-b.x)-kScale);
                const int64_t dz=std::max(0,std::abs(u.z-b.z)-kScale);
                const auto d=dx*dx+dz*dz;
                if (d<distance) { distance=d; building=&b; }
            }
            if (building) {
                u.target_structure_id=building->id;
                if (distance<=attack_range2) {
                    if (u.cooldown==0) { structure_damage[building->id-1]+=8; u.cooldown=10; }
                    goal={u.x,u.z};
                } else if (u.order==Order::AttackMove) {
                    const auto approach=service_point(u,building->x,building->z,kScale);
                    if (approach.x>=0) goal=approach;
                }
            }
        }
        if (u.order == Order::Stop || u.order == Order::Hold) { continue; }
        const nav::Point here{u.x,u.z};
        if (goal == here) { u.path.clear(); u.detour.clear(); u.blocked_ticks=0; u.route_goal={-1,-1}; u.next_x=u.x; u.next_z=u.z; continue; }
        if (!(u.route_goal == goal)) {
            u.path = navigation_.route(here, goal); u.route_goal=goal;
            // Pursuit updates the effective goal every tick. Keep a safe local
            // maneuver and its wait history; explicit orders clear both in apply.
        }
        while (!u.path.empty() && u.path.front() == here) u.path.erase(u.path.begin());
        if (u.path.empty()) { u.next_x=u.x; u.next_z=u.z; continue; }
        const bool had_detour=!u.detour.empty();
        while (!u.detour.empty() && u.detour.front()==here) u.detour.erase(u.detour.begin());
        if (had_detour && u.detour.empty()) {
            // The maneuver can end off the original visibility segment. Refresh
            // from this position rather than following a stale terrain tangent.
            u.path=navigation_.route(here,goal);
            if (u.path.empty()) { u.next_x=u.x; u.next_z=u.z; continue; }
        }
        const auto waypoint = u.detour.empty()?u.path.front():u.detour.front();
        u.next_x=waypoint.x; u.next_z=waypoint.z;
        step_candidates(here,waypoint,candidates);
        // Either participant can move by movement_speed in this tick. Query
        // tick-start centers conservatively for every candidate and other sweep.
        constexpr int reach=2*unit_radius+2*movement_speed;
        spatial.query({u.x-reach,u.z-reach,u.x+reach,u.z+reach},nearby);
        uint32_t blocker=0;
        const auto occupied=[&](const Unit& other,int padding=0) {
            const auto old=tick_start[other.id-1];
            return nav::Rect{std::min(old.x,other.x)-2*unit_radius-padding,
                std::min(old.z,other.z)-2*unit_radius-padding,std::max(old.x,other.x)+2*unit_radius+padding,
                std::max(old.z,other.z)+2*unit_radius+padding};
        };
        for (const auto candidate : candidates) {
            bool free = navigation_.clear(here,candidate);
            for (const auto id : nearby) {
                if (!free) break;
                const auto& other=units_[id-1];
                if (other.id==u.id) continue;
                // Protect the entire other-unit sweep, including presentation interpolation.
                free=nav::segment_clear(here,candidate,occupied(other));
                if (!free && !blocker) blocker=id;
            }
            if (free) { u.x=candidate.x; u.z=candidate.z; u.moving=!(candidate==here); break; }
        }
        if (u.moving) { u.blocked_ticks=0; }
        else if (blocker && ++u.blocked_ticks>=2) {
            if (u.order==Order::Move) {
                attempted_detours[u.id-1]=u.detour;
                reconsidered_detour[u.id-1]=1;
            }
            // A bounded local maneuver around the first blocking sweep. At most
            // four corners and two segments are considered, with a right-hand
            // preference for equally short opposing encounters. Every actual
            // step still rechecks current sweeps; future waypoints reserve nothing.
            const auto& obstacle=units_[blocker-1];
            const auto exact=occupied(obstacle);
            const bool occupied_goal=goal.x>exact.min_x && goal.x<exact.max_x &&
                goal.z>exact.min_z && goal.z<exact.max_z;
            if (occupied_goal && (obstacle.order==Order::Stop || obstacle.order==Order::Hold)) {
                if (!u.detour.empty()) u.route_goal={-1,-1};
                u.detour.clear(); u.blocked_ticks=0;
            } else if (u.detour.empty() || u.blocked_ticks>=8) {
                const auto box=occupied(obstacle,16);
                const std::array<nav::Point,4> corners{{{box.min_x,box.min_z},{box.max_x,box.min_z},
                    {box.max_x,box.max_z},{box.min_x,box.max_z}}};
                const auto clear=[&](nav::Point a,nav::Point b) {
                    if (!navigation_.clear(a,b)) return false;
                    constexpr int pad=2*unit_radius+movement_speed;
                    spatial.query({std::min(a.x,b.x)-pad,std::min(a.z,b.z)-pad,
                        std::max(a.x,b.x)+pad,std::max(a.z,b.z)+pad},local);
                    for (const auto id:local) if (id!=u.id && !nav::segment_clear(a,b,occupied(units_[id-1]))) return false;
                    return true;
                };
                const auto destination=u.path.front();
                const int64_t dx=int64_t(destination.x)-here.x,dz=int64_t(destination.z)-here.z;
                const auto length=[](nav::Point a,nav::Point b) {
                    const int64_t x=int64_t(a.x)-b.x,z=int64_t(a.z)-b.z;
                    return nav::ceil_sqrt(static_cast<uint64_t>(x*x+z*z));
                };
                std::vector<nav::Point> best;
                uint64_t best_cost=std::numeric_limits<uint64_t>::max();
                int best_side=2;
                for (size_t a=0;a<4;++a) for (size_t b=0;b<4;++b) {
                    if (a!=b && (a+2)%4==b) continue;
                    const auto first=corners[a],last=corners[b];
                    // Finish on the far half of the obstacle, avoiding repeated
                    // near-corner maneuvers that make no progress past a blocker.
                    if (dx*(int64_t(last.x)-obstacle.x)+dz*(int64_t(last.z)-obstacle.z)<0 ||
                        last==here || !navigation_.clear(last,destination) || !clear(here,first) || !clear(first,last)) continue;
                    const auto cost=length(here,first)+length(first,last)+length(last,destination);
                    const int side=dx*(int64_t(first.z)-here.z)-dz*(int64_t(first.x)-here.x)<=0?0:1;
                    if (std::tie(cost,side)<std::tie(best_cost,best_side)) {
                        best_cost=cost; best_side=side; best={first};
                        if (!(last==first)) best.push_back(last);
                    }
                }
                if (best.empty() && !u.detour.empty()) u.route_goal={-1,-1};
                u.detour=std::move(best); u.blocked_ticks=0;
            }
        }
        if (u.order == Order::Move && u.x == u.goal_x && u.z == u.goal_z) u.order = Order::Stop;
    }
    // A stopped follower must not treat its leader's entire swept bounding box
    // as occupied at every instant. Resolve a bounded dependency transaction
    // after the ordinary combat/movement decisions, retaining their ordering.
    // Only unfinished Move units can join; firing, Hold, Stop and units which
    // already moved this tick are immutable obstacles with their actual sweep.
    std::vector<nav::Point> proposed(units_.size());
    std::vector<uint8_t> planned(units_.size(),0);
    std::vector<uint32_t> members;
    members.reserve(32);
    uint32_t attempts=0, starts=0;
    const auto relative_clear=[&](uint32_t a,nav::Point end_a,uint32_t b,nav::Point end_b) {
        const auto old_a=tick_start[a],old_b=tick_start[b];
        return nav::segment_clear({old_a.x-old_b.x,old_a.z-old_b.z},
            {end_a.x-end_b.x,end_a.z-end_b.z},
            {-2*unit_radius,-2*unit_radius,2*unit_radius,2*unit_radius});
    };
    const auto eligible=[&](uint32_t index) {
        const auto& u=units_[index];
        return u.hp>0 && u.order==Order::Move && !u.moving &&
            (u.x!=u.goal_x || u.z!=u.goal_z) && !u.path.empty();
    };
    std::function<bool(uint32_t)> plan;
    plan=[&](uint32_t index) {
        if (!eligible(index) || planned[index] || members.size()>=32 || attempts>=2048) return false;
        const auto& u=units_[index];
        const nav::Point here{u.x,u.z};
        std::vector<nav::Point> steps;
        step_candidates(here,{u.next_x,u.next_z},steps);
        std::vector<uint32_t> neighbors;
        constexpr int reach=2*unit_radius+2*movement_speed;
        spatial.query({u.x-reach,u.z-reach,u.x+reach,u.z+reach},neighbors);
        const auto checkpoint=members.size();
        for (const auto step:steps) {
            if (attempts>=2048) break;
            ++attempts;
            if (step==here || !navigation_.clear(here,step)) continue;
            proposed[index]=step; planned[index]=1; members.push_back(index);
            bool clear=true;
            for (const auto id:neighbors) {
                const auto other=id-1;
                if (other==index) continue;
                const auto& v=units_[other];
                auto end=planned[other]?proposed[other]:nav::Point{v.x,v.z};
                if (relative_clear(index,step,other,end)) continue;
                // A tentative ancestor closes a dependency cycle. Its endpoint
                // cannot change beneath this trial; retry another candidate.
                if (planned[other] || !plan(other)) { clear=false; break; }
                end=proposed[other];
                if (!relative_clear(index,step,other,end)) { clear=false; break; }
            }
            if (clear) return true;
            while (members.size()>checkpoint) {
                planned[members.back()]=0; members.pop_back();
            }
        }
        return false;
    };
    // Rotate the starting identity instead of granting permanent priority to
    // the first blocked unit. Counts are fixed, never driven by elapsed time.
    for (size_t n=0;n<units_.size() && starts<16 && attempts<2048;++n) {
        const auto index=static_cast<uint32_t>((n+tick_)%units_.size());
        if (!eligible(index)) continue;
        ++starts;
        if (!plan(index)) continue;
        // Validate the complete transaction again. Recursive trials can add a
        // participant after an earlier sibling was planned; no partial subset
        // may commit with a dependency's assumed displacement removed.
        bool clear=true;
        for (const auto member:members) {
            const auto old=tick_start[member];
            std::vector<uint32_t> neighbors;
            constexpr int reach=2*unit_radius+2*movement_speed;
            spatial.query({old.x-reach,old.z-reach,old.x+reach,old.z+reach},neighbors);
            for (const auto id:neighbors) if (id-1!=member) {
                const auto other=id-1;
                const auto& v=units_[other];
                const auto end=planned[other]?proposed[other]:nav::Point{v.x,v.z};
                if (!relative_clear(member,proposed[member],other,end)) { clear=false; break; }
            }
            if (!clear) break;
        }
        if (clear) for (const auto member:members) {
            auto& u=units_[member];
            u.x=proposed[member].x; u.z=proposed[member].z;
            u.moving=true; u.blocked_ticks=0;
            // This retry succeeded along the waypoint selected before the
            // failed ordinary step. Discard only the replacement maneuver
            // searched afterward; retain any maneuver actually being followed.
            if (reconsidered_detour[member]) u.detour=std::move(attempted_detours[member]);
            if (u.x==u.goal_x && u.z==u.goal_z) u.order=Order::Stop;
        }
        for (const auto member:members) planned[member]=0;
        members.clear();
    }
    for (size_t i = 0; i < units_.size(); ++i) {
        units_[i].hp = std::max(0, units_[i].hp - damage[i]);
        if (units_[i].hp == 0) units_[i].moving = false;
    }
    bool destroyed=false;
    for (size_t i=0;i<structures_.size();++i) {
        auto& b=structures_[i];
        if (b.hp<=0) continue;
        b.hp=std::max(0,b.hp-structure_damage[i]);
        if (b.hp==0) {
            // Destruction forfeits every paid item, including blocked exits.
            // Tombstones retain stable IDs; no resource refund is generated.
            b.production_queue=0; b.production_ticks=0; b.spawn_blocked=false;
            destroyed=true;
        }
    }
    if (destroyed) rebuild_navigation();
    if (map_==Map::Economy && winner()!=-1)
        for (auto& u:units_) { u.moving=false; u.target_id=0; u.target_structure_id=0; }
    ++tick_;
}

int Sim::winner() const {
    std::array<bool, 2> live{};
    if (map_==Map::Economy) {
        for (const auto& b:structures_) if (b.hp>0 && b.kind==StructureKind::Anchor) live[b.player]=true;
    } else for (const auto& u : units_) if (u.hp > 0) live[u.player] = true;
    if (live[0] && live[1]) return -1;
    if (live[0]) return 0;
    if (live[1]) return 1;
    return 2;
}

std::vector<uint8_t> serialize_command(const Command& c) {
    if (!canonical(c)) return {};
    std::vector<uint8_t> out{'V','F','C',1};
    append(out, c.tick); append(out, c.sequence);
    out.push_back(c.player); out.push_back(static_cast<uint8_t>(c.order));
    append(out, static_cast<uint32_t>(c.x)); append(out, static_cast<uint32_t>(c.z));
    append(out, static_cast<uint32_t>(c.units.size()));
    for (const auto id : c.units) append(out, id);
    return out;
}

bool deserialize_command(std::span<const uint8_t> bytes, Command& out) {
    if (bytes.size() < 26 || bytes[0] != 'V' || bytes[1] != 'F' || bytes[2] != 'C' || bytes[3] != 1) return false;
    size_t pos = 4;
    const auto read = [&]() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= uint32_t(bytes[pos++]) << (i * 8);
        return value;
    };
    Command c;
    c.tick = read(); c.sequence = read(); c.player = bytes[pos++]; c.order = static_cast<Order>(bytes[pos++]);
    const auto x = read(), z = read(), count = read();
    if (x >= kMaxMapSize * kScale || z >= kMaxMapSize * kScale || count == 0 || count > 256 || bytes.size() != 26 + count * 4) return false;
    c.x = static_cast<int32_t>(x); c.z = static_cast<int32_t>(z);
    for (uint32_t i = 0; i < count; ++i) c.units.push_back(read());
    if (!canonical(c)) return false;
    out = std::move(c); return true;
}

uint64_t Sim::state_hash() const {
    uint64_t h = 14695981039346656037ull;
    const auto add = [&](uint64_t v) { for (int i = 0; i < 8; ++i) { h ^= (v >> (8 * i)) & 255; h *= 1099511628211ull; } };
    add(kProtocolVersion); add(static_cast<uint32_t>(map_)); add(width()); add(height());
    add(tick_); add(rng_); add(last_sequence_[0]); add(last_sequence_[1]); add(units_.size());
    for (const auto& u : units_) {
        add(static_cast<uint8_t>(u.kind)); add(u.cargo); add(u.resource_id); add(u.build_id); add(u.work_ticks); add(u.returning);
        add(u.id); add(u.player); add(u.x); add(u.z); add(u.hp); add(static_cast<uint8_t>(u.order));
        add(u.target_id); add(u.target_structure_id); add(u.cooldown); add(u.moving); add(u.goal_x); add(u.goal_z); add(u.next_x); add(u.next_z);
        add(u.route_goal.x); add(u.route_goal.z); add(u.path.size());
        for (const auto& point : u.path) { add(point.x); add(point.z); }
        add(u.blocked_ticks); add(u.detour.size());
        for (const auto& point : u.detour) { add(point.x); add(point.z); }
    }
    for (int p=0;p<2;++p) { add(salvage_[p]); add(static_cast<uint8_t>(results_[p])); add(result_sequences_[p]); }
    add(structures_.size());
    for (const auto& b:structures_) { add(b.id); add(b.player); add(static_cast<uint8_t>(b.kind)); add(b.x); add(b.z); add(b.hp); add(b.build_ticks);
        add(b.production_queue); add(b.production_ticks); add(b.spawn_blocked); }
    add(deposits_.size());
    for (const auto& d:deposits_) { add(d.id); add(d.x); add(d.z); add(d.remaining); }
    return h;
}

uint64_t Sim::hash() const {
    // Preserve the legacy replay hash, which includes queued commands.
    uint64_t h = state_hash();
    const auto add = [&](uint64_t v) { for (int i = 0; i < 8; ++i) { h ^= (v >> (8 * i)) & 255; h *= 1099511628211ull; } };
    add(pending_.size());
    for (const auto& c : pending_) for (auto b : serialize_command(c)) add(b);
    return h;
}

std::vector<Command> make_ai_commands(const Sim& sim, uint8_t player, uint32_t& sequence) {
    if (player >= 2 || sim.winner() != -1 || sim.tick() % 20 != 0) return {};
    if (sim.map()==Map::Economy) {
        std::vector<Command> commands;
        const auto emit=[&](Order order,std::vector<uint32_t> ids,int32_t x=0,int32_t z=0) {
            commands.push_back({sim.tick(),++sequence,player,order,std::move(ids),x,z});
        };
        const Structure* anchor=nullptr;
        const Structure* enemy=nullptr;
        const Structure* foundry=nullptr;
        for (const auto& b:sim.structures()) if (b.hp>0) {
            if (b.kind==StructureKind::Anchor) { if (b.player==player) anchor=&b; else enemy=&b; }
            if (b.player==player && b.kind==StructureKind::Foundry && !foundry) foundry=&b;
        }
        if (!anchor || !enemy) return {};
        const Unit* builder=nullptr;
        bool constructing=false;
        for (const auto& u:sim.units()) if (u.hp>0 && u.player==player && u.kind==UnitKind::Worker) {
            if (!builder) builder=&u;
            if (foundry && u.order==Order::Build && u.build_id==foundry->id) constructing=true;
        }
        uint32_t assigned_builder=0;
        if (builder && foundry && foundry->build_ticks<kBuildTicks && !constructing) {
            emit(Order::Build,{builder->id},foundry->x,foundry->z); assigned_builder=builder->id;
        } else if (builder && !foundry && sim.salvage(player)>=kFoundryCost) {
            // Prefer a forward site away from the mining lane. Enumerate a
            // bounded anchor-linked fallback if units or ruins occupy that site.
            const int direction=player==0?1:-1;
            bool placed=false;
            for (int z_offset : {3,4,5,0,-3,-4,-5}) {
                for (int x_offset : {5,4,3,0,-3,-4,-5}) {
                    const int x=anchor->x+direction*x_offset*kScale,z=anchor->z+z_offset*kScale;
                    if (!sim.can_build(player,x,z)) continue;
                    emit(Order::Build,{builder->id},x,z); assigned_builder=builder->id; placed=true; break;
                }
                if (placed) break;
            }
        }
        for (const auto& u:sim.units()) if (u.hp>0 && u.player==player && u.kind==UnitKind::Worker &&
            u.id!=assigned_builder && u.order!=Order::Build && u.order!=Order::Gather && u.order!=Order::ReturnCargo) {
            const Deposit* nearest=nullptr;
            int64_t distance=std::numeric_limits<int64_t>::max();
            for (const auto& d:sim.deposits()) if (d.remaining>0) {
                const int64_t dx=int64_t(u.x)-d.x,dz=int64_t(u.z)-d.z;
                if (dx*dx+dz*dz<distance) { distance=dx*dx+dz*dz; nearest=&d; }
            }
            if (nearest) emit(Order::Gather,{u.id},nearest->x,nearest->z);
            else if (u.cargo>0) emit(Order::ReturnCargo,{u.id});
        }
        if (foundry && foundry->build_ticks>=kBuildTicks && foundry->production_queue<kProductionQueueLimit &&
            sim.salvage(player)>=kStriderCost && sim.population_used(player)+sim.population_reserved(player)<kPopulationCap &&
            sim.units().size()+sim.population_reserved(0)+sim.population_reserved(1)<kLifetimeUnitLimit)
            emit(Order::TrainStrider,{foundry->id});
        std::vector<uint32_t> army;
        for (const auto& u:sim.units()) if (u.hp>0 && u.player==player && u.kind==UnitKind::Strider && u.order!=Order::AttackMove)
            army.push_back(u.id);
        if (!army.empty()) emit(Order::AttackMove,std::move(army),enemy->x,enemy->z);
        return commands;
    }
    Command c;
    c.tick = sim.tick(); c.sequence = ++sequence; c.player = player; c.order = Order::AttackMove;
    const Unit* enemy = nullptr;
    for (const auto& u : sim.units()) {
        if (u.hp <= 0) continue;
        if (u.player == player) c.units.push_back(u.id);
        else if (!enemy) enemy = &u;
    }
    if (!enemy || c.units.empty()) return {};
    c.x = enemy->x; c.z = enemy->z;
    return {std::move(c)};
}
}
