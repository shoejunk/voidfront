#include "voidfront_sim.hpp"
#include "spatial.hpp"
#include <algorithm>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace vf {
int map_width(Map map) {
    if (map==Map::Foundry) return kMapWidth;
    if (map==Map::Economy) return 64;
    if (map==Map::Scale128) return 128;
    throw std::invalid_argument("unsupported map");
}
int map_height(Map map) { return map==Map::Economy?48:(map==Map::Foundry?kMapHeight:map_width(map)); }
const std::vector<nav::Rect>& map_terrain(Map map) {
    static const std::vector<nav::Rect> foundry{{15*kScale,3*kScale,17*kScale,9*kScale},
        {15*kScale,16*kScale,17*kScale,21*kScale}};
    static const std::vector<nav::Rect> scale{{63*kScale,8*kScale,65*kScale,60*kScale},
        {63*kScale,68*kScale,65*kScale,120*kScale}};
    static const std::vector<nav::Rect> economy{{15*kScale,3*kScale,17*kScale,9*kScale},
        {15*kScale,16*kScale,17*kScale,21*kScale},
        {31*kScale,5*kScale,33*kScale,19*kScale},
        {31*kScale,29*kScale,33*kScale,43*kScale},
        {47*kScale,27*kScale,49*kScale,32*kScale},
        {47*kScale,39*kScale,49*kScale,45*kScale}};
    if (map==Map::Foundry) return foundry;
    if (map==Map::Economy) return economy;
    if (map==Map::Scale128) return scale;
    throw std::invalid_argument("unsupported map");
}
namespace {
nav::Point home_salvage(uint8_t player) {
    return {(player==0?7:56)*kScale+kScale/2,(player==0?7:40)*kScale+kScale/2};
}
nav::Rect terrain_bounds(Map map) { return {kScale,kScale,(map_width(map)-1)*kScale,(map_height(map)-1)*kScale}; }
constexpr int unit_radius = 64, movement_speed = 32, strider_range_tiles = 3, strider_acquire_tiles = 6;
constexpr int worker_sight_tiles=7;
int64_t distance2(const Unit& a, const Unit& b) {
    const int64_t dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}
bool canonical(const Command& c) {
    return c.player < 2 && c.sequence > 0 && c.order <= Order::TrainWorker &&
        (!is_structure_order(c.order) || (c.units.size()==1 && c.x==0 && c.z==0)) &&
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
        if (ea!=eb) return ea<eb;
        // Ties break on the offset signed by travel direction, so a 180-degree rotated
        // trip (negated delta and offsets) selects the rotated step.
        const int sign=dx>0||(dx==0&&dz>0)?1:-1;
        return std::make_tuple(sign*(a.x-here.x),sign*(a.z-here.z))<std::make_tuple(sign*(b.x-here.x),sign*(b.z-here.z));
    });
}

bool holds_position(const Unit& unit) {
    return unit.order==Order::Stop || unit.order==Order::Hold;
}

std::vector<nav::Point> detour_around_group(const nav::World& world,const SpatialIndex& spatial,
        const std::vector<Unit>& units,const std::vector<nav::Point>& tick_start,
        const Unit& mover,const Unit& obstacle,nav::Point goal,bool replacing_maneuver) {
    constexpr int lookahead=4*kScale, margin=kScale;
    std::vector<uint32_t> nearby;
    if (replacing_maneuver) {
        // Keep the existing maneuver when another unit can change the local
        // geometry. Include the entire spatial buckets touching the planning
        // window so a moving neighbor at its edge also retains priority.
        constexpr int window=lookahead+margin;
        spatial.query({mover.x-window,mover.z-window,mover.x+window,mover.z+window},nearby);
        for (const auto id:nearby) {
            const auto& other=units[id-1];
            if (id!=mover.id && (!holds_position(other) || nav::Point{other.x,other.z}!=tick_start[id-1]))
                return {};
        }
    }
    // Forbidden center rectangles meet when two held centers are four radii
    // apart. Include the neighbors of the first obstruction in stable ID order.
    constexpr int reach=4*unit_radius, clearance=2*unit_radius;
    spatial.query({obstacle.x-reach,obstacle.z-reach,obstacle.x+reach,obstacle.z+reach},nearby);
    std::vector<nav::Rect> blockers;
    for (const auto id:nearby) {
        const auto& other=units[id-1];
        if (id==mover.id || !holds_position(other) || nav::Point{other.x,other.z}!=tick_start[id-1] ||
            std::max(std::abs(other.x-obstacle.x),std::abs(other.z-obstacle.z))>reach) continue;
        blockers.push_back({other.x-clearance,other.z-clearance,other.x+clearance,other.z+clearance});
        if (blockers.size()>8) return {};
    }
    if (blockers.size()<2) return {};
    return world.local_detour({mover.x,mover.z},goal,std::move(blockers),lookahead,margin);
}
}

Sim::Sim(uint32_t seed, uint32_t count, Map map) : map_(map), rng_(seed ? seed : 1), navigation_(terrain_bounds(map), map_terrain(map), unit_radius) {
    if (count < 1 || count > 250) throw std::invalid_argument("units_per_team must be 1..250");
    if (map==Map::Economy) {
        for (uint8_t p=0;p<2;++p) {
            structures_.push_back({uint32_t(p)+1,p,StructureKind::Anchor,(p==0?4:59)*kScale+128,(p==0?12:35)*kScale+128});
            const auto mine=home_salvage(p);
            deposits_.push_back({uint32_t(p)+1,mine.x,mine.z,2000});
            for (int n=0;n<3;++n) {
                Unit u;
                u.id=static_cast<uint32_t>(units_.size())+1; u.player=p; u.kind=UnitKind::Worker;
                u.x=u.goal_x=u.next_x=(p==0?6:57)*kScale+128;
                u.z=u.goal_z=u.next_z=(p==0?11+n:36-n)*kScale+128;
                units_.push_back(u);
            }
        }
        // The map is a 180-degree rotation about (32,24): cell x<->63-x, z<->47-z, so bases,
        // mineral and flux deposits are exactly equidistant by construction.
        // Deposit IDs are 1-based indices into deposits_.
        deposits_.push_back({3,29*kScale+128,24*kScale+128,1000,1});
        deposits_.push_back({4,34*kScale+128,23*kScale+128,1000,1});
        rebuild_navigation();
        update_vision();
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

uint8_t Sim::visibility(uint8_t player,int x,int z) const {
    if (player>=2 || x<0 || z<0 || x>=width() || z>=height()) return 0;
    return map_==Map::Economy?vision_[player][z*width()+x]:2;
}
bool Sim::knows_depleted(uint8_t player,const Deposit& deposit) const {
    return player<2 && ((deposit.depletion_seen_by & (1u<<player)) ||
        (visibility(player,deposit.x/kScale,deposit.z/kScale)==2 && deposit.remaining==0));
}
void Sim::update_vision() {
    if (map_!=Map::Economy) return;
    for (auto& cells:vision_) {
        cells.resize(width()*height(),0);
        for (auto& cell:cells) if (cell==2) cell=1;
    }
    const auto reveal=[&](uint8_t player,int32_t x,int32_t z,int radius) {
        const int cx=x/kScale,cz=z/kScale;
        for (int row=std::max(0,cz-radius);row<=std::min(height()-1,cz+radius);++row)
            for (int col=std::max(0,cx-radius);col<=std::min(width()-1,cx+radius);++col) {
                const int64_t dx=int64_t(col*kScale+kScale/2)-x,dz=int64_t(row*kScale+kScale/2)-z;
                if (dx*dx+dz*dz<=int64_t(radius*kScale)*(radius*kScale)) vision_[player][row*width()+col]=2;
            }
    };
    // Shared circular team vision. Ridges block movement, not sight in this increment.
    for (const auto& u:units_) if (u.hp>0) reveal(u.player,u.x,u.z,u.kind==UnitKind::Worker?worker_sight_tiles:9);
    for (const auto& b:structures_) if (b.hp>0) reveal(b.player,b.x,b.z,b.kind==StructureKind::Anchor?10:7);
    for (auto& d:deposits_) if (d.remaining==0)
        for (uint8_t p=0;p<2;++p) if (visibility(p,d.x/kScale,d.z/kScale)==2)
            d.depletion_seen_by|=static_cast<uint8_t>(1u<<p);
    for (uint8_t p=0;p<2;++p)
        if (knows_depleted(p,deposits_[p])) update_salvage_guard(p);
    update_mining_retreats();
}

void Sim::update_salvage_guard(uint8_t player) {
    const auto patch=home_salvage(1-player);
    auto& cell=salvage_guard_cell_[player];
    const Unit* nearest=nullptr;
    int64_t nearest_distance=std::numeric_limits<int64_t>::max();
    for (const auto& u:units_) {
        if (u.player==player || u.hp<=0 || u.kind==UnitKind::Worker || visibility(player,u.x/kScale,u.z/kScale)!=2) continue;
        // Allow two tiles for the miner's approach and perimeter service slot.
        const int64_t reach=((u.kind==UnitKind::Lancer?kLancerRangeTiles:strider_range_tiles)+2)*kScale;
        const int64_t dx=u.x-patch.x,dz=u.z-patch.z,distance=dx*dx+dz*dz;
        if (distance<=reach*reach && distance<nearest_distance) {
            nearest=&u; nearest_distance=distance;
        }
    }
    if (nearest) {
        cell={nearest->x/kScale,nearest->z/kScale};
        salvage_guard_seen_tick_[player]=tick_;
    } else if (cell.x>=0 && (visibility(player,cell.x,cell.z)==2 || tick_-salvage_guard_seen_tick_[player]>=30*kTicksPerSecond)) {
        // Recheck a dark approach after thirty seconds, even with no surviving army.
        cell={-1,-1};
        salvage_guard_seen_tick_[player]=0;
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
    if (is_structure_order(command.order)) {
        const auto id=command.units.front();
        if (map_!=Map::Economy || id>structures_.size() || structures_[id-1].player!=command.player) return false;
    } else for (const auto id : command.units)
        if (id > units_.size() || units_[id - 1].player != command.player) return false;
    pending_.push_back(std::move(command));
    std::sort(pending_.begin(), pending_.end(), command_less);
    return true;
}

// Enumerate Manhattan rings once in row-major order. This preserves the
// original exhaustive search's exact distance/row/column ties without
// rescanning every map cell for each selected unit.
// `rotated` reverses the row and column tie order (the 180-degree image of the
// default), so player 1 on the rotation-symmetric economy map gets the mirrored
// slot set of player 0.
std::vector<nav::Point> Sim::formation_slots(int32_t x0,int32_t z0,size_t count,bool rotated) const {
    std::vector<nav::Point> slots;
    const int rx=x0/kScale,rz=z0/kScale;
    for (int radius=0;radius<width()+height() && slots.size()<count;++radius) {
        const int low=std::max(0,rz-radius),high=std::min(height()-1,rz+radius);
        for (int step=0;step<=high-low && slots.size()<count;++step) {
            const int z=rotated?high-step:low+step;
            const int dx=radius-std::abs(z-rz);
            for (const int x : {rotated?rx+dx:rx-dx,rotated?rx-dx:rx+dx}) {
                if (!blocked(x,z)) slots.push_back({x*kScale+kScale/2,z*kScale+kScale/2});
                if (dx==0 || slots.size()==count) break;
            }
        }
    }
    return slots;
}

void Sim::advance_queue(Unit& u) {
    const auto leg=u.queue.front();
    u.queue.erase(u.queue.begin());
    u.order=leg.order; u.target_id=0; u.target_structure_id=0;
    u.path.clear(); u.next_x=u.x; u.next_z=u.z;
    u.detour.clear(); u.blocked_ticks=0; u.route_goal={-1,-1};
    u.goal_x=leg.x; u.goal_z=leg.z;
}

// Shift-queued movement: a unit already moving appends a leg (bounded);
// any other unit starts the leg immediately, exactly like the plain order.
void Sim::apply_queued(const Command& c) {
    const Order order=c.order==Order::QueueMove?Order::Move:Order::AttackMove;
    const auto slots=formation_slots(c.x,c.z,c.units.size(),map_==Map::Economy && c.player==1);
    size_t next_slot=0;
    for (const auto id : c.units) {
        auto& u=units_[id-1];
        if (u.hp<=0) continue;
        nav::Point goal{-1,-1};
        if (c.units.size()==1 && navigation_.valid({c.x,c.z})) goal={c.x,c.z};
        else if (next_slot<slots.size()) goal=slots[next_slot++];
        if (goal.x<0) continue;
        const bool busy=u.order==Order::Move || u.order==Order::AttackMove;
        if (busy) {
            if (u.queue.size()<kOrderQueueLimit) u.queue.push_back({order,goal.x,goal.z});
            continue;
        }
        u.resource_id=0; u.build_id=0; u.work_ticks=0; u.returning=false;
        u.queue.clear();
        u.queue.push_back({order,goal.x,goal.z});
        advance_queue(u);
    }
}

void Sim::apply(const Command& c) {
    last_sequence_[c.player] = c.sequence;
    // Already-buffered lockstep inputs still consume their sequence after the
    // result, but can no longer change the economy or battlefield.
    if (map_==Map::Economy && winner()!=-1) return;
    if (c.order==Order::QueueMove || c.order==Order::QueueAttackMove) {
        if (map_==Map::Economy) { results_[c.player]=CommandResult::Accepted; result_sequences_[c.player]=c.sequence; }
        apply_queued(c); return;
    }
    if (static_cast<uint8_t>(c.order)>=4) {
        if (c.order==Order::ReturnCargo && map_==Map::Economy && knows_depleted(c.player,deposits_[c.player]))
            for (const auto id:c.units) remember_mining_retreat(units_[id-1]);
        apply_economy(c); return;
    }
    if (map_==Map::Economy) { results_[c.player]=CommandResult::Accepted; result_sequences_[c.player]=c.sequence; }
    std::vector<nav::Point> slots;
    if (c.order==Order::Move || c.order==Order::AttackMove) slots=formation_slots(c.x,c.z,c.units.size(),map_==Map::Economy && c.player==1);
    size_t next_slot=0;
    for (const auto id : c.units) {
        auto& u = units_[id - 1];
        if (u.hp <= 0) continue;
        u.resource_id=0; u.build_id=0; u.work_ticks=0; u.returning=false;
        u.queue.clear();
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
    update_vision();
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
    constexpr int64_t attack_range2 = int64_t(strider_range_tiles * kScale) * (strider_range_tiles * kScale);
    constexpr int64_t acquire_range2 = int64_t(strider_acquire_tiles * kScale) * (strider_acquire_tiles * kScale);
    for (auto& u : units_) {
        u.moving = false; u.target_id = 0; u.target_structure_id=0;
        if (u.hp <= 0) continue;
        if (u.cooldown > 0) --u.cooldown;
        const Unit* target = nullptr;
        const bool lancer=u.kind==UnitKind::Lancer;
        const int64_t unit_attack2=lancer?int64_t(kLancerRangeTiles*kScale)*(kLancerRangeTiles*kScale):attack_range2;
        const int64_t unit_acquire2=lancer?int64_t(kLancerAcquireTiles*kScale)*(kLancerAcquireTiles*kScale):acquire_range2;
        const int unit_damage=lancer?kLancerDamage:8;
        const uint16_t unit_cooldown=lancer?kLancerCooldown:10;
        int64_t nearest = unit_acquire2 + 1;
        if (u.kind!=UnitKind::Worker && u.order != Order::Move) {
            const int reach=(lancer?kLancerAcquireTiles:strider_acquire_tiles)*kScale+movement_speed;
            spatial.query({u.x-reach,u.z-reach,u.x+reach,u.z+reach},nearby);
            for (const auto id : nearby) {
                const auto& other=units_[id-1];
                if (other.hp <= 0 || other.player == u.player) continue;
                const auto& op = tick_start[other.id-1];  // start-of-tick positions: processing order must not matter
                const int64_t ddx = op.x - u.x, ddz = op.z - u.z;
                const auto d = ddx*ddx + ddz*ddz;
                bool better = d < nearest;
                if (!better && d == nearest && target) {
                    // Exact ties resolve by the offset in the attacker's own frame
                    // (player 1 negates), so mirrored positions pick mirrored targets
                    // instead of favoring the lower unit ID.
                    const int sign = u.player == 0 ? 1 : -1;
                    const auto ox = sign*(op.x-u.x), oz = sign*(op.z-u.z);
                    const auto& tp = tick_start[target->id-1];
                    const auto tx = sign*(tp.x-u.x), tz = sign*(tp.z-u.z);
                    better = ox < tx || (ox == tx && (oz < tz || (oz == tz && other.id < target->id)));
                }
                if (better) { nearest = d; target = &other; }
            }
        }
        nav::Point goal{u.goal_x, u.goal_z};
        if (target) {
            u.target_id = target->id;
            if (nearest <= unit_attack2) {
                if (u.cooldown == 0) { damage[target->id - 1] += unit_damage; u.cooldown = unit_cooldown; }
                goal = {u.x, u.z};
            } else if (u.order == Order::AttackMove) goal = {target->x, target->z};
        }
        // Units have priority; otherwise acquire the nearest enemy building by
        // squared distance to its footprint, resolving ties by stable ID.
        // Structures have a separate ID space, exposed separately in snapshots.
        if (!target && u.kind!=UnitKind::Worker && u.order!=Order::Move) {
            const Structure* building=nullptr;
            int64_t distance=unit_acquire2+1;
            for (const auto& b:structures_) if (b.hp>0 && b.player!=u.player) {
                const int64_t dx=std::max(0,std::abs(u.x-b.x)-kScale);
                const int64_t dz=std::max(0,std::abs(u.z-b.z)-kScale);
                const auto d=dx*dx+dz*dz;
                if (d<distance) { distance=d; building=&b; }
            }
            if (building) {
                u.target_structure_id=building->id;
                if (distance<=unit_attack2) {
                    if (u.cooldown==0) { structure_damage[building->id-1]+=unit_damage; u.cooldown=unit_cooldown; }
                    goal={u.x,u.z};
                } else if (u.order==Order::AttackMove) {
                    const auto approach=service_point(u,building->x,building->z,kScale);
                    if (approach.x>=0) goal=approach;
                }
            }
        }
        if (!u.queue.empty() && (u.order==Order::Move || u.order==Order::AttackMove) && !target && u.target_structure_id==0 &&
            u.x==u.goal_x && u.z==u.goal_z) { advance_queue(u); goal={u.goal_x,u.goal_z}; }
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
        else if (!blocker && !navigation_.clear(here,waypoint)) {
            // Rounded steps can drift off a terrain tangent. Once terrain
            // rejects every step, replan the stale leg from the actual position.
            u.path.clear(); u.detour.clear(); u.blocked_ticks=0;
            u.route_goal={-1,-1}; u.next_x=u.x; u.next_z=u.z;
        }
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
            if (occupied_goal && holds_position(obstacle)) {
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
                // A padded corner can be reachable while its onward leg
                // crosses the same held footprint, sending the unit in a loop.
                if (holds_position(obstacle) &&
                    (best.empty() || !nav::segment_clear(best.back(),destination,exact))) {
                    // Limit larger searches to eight identities per tick. Two
                    // consecutive ticks per window cover both blocked-step phases.
                    constexpr size_t slots=8;
                    const auto first=(uint64_t(tick_/2)*slots)%units_.size();
                    if ((u.id-1+units_.size()-first)%units_.size()<slots) {
                        auto replacement=detour_around_group(navigation_,spatial,units_,tick_start,
                                                             u,obstacle,goal,!best.empty());
                        if (!replacement.empty()) best=std::move(replacement);
                    }
                }
                if (best.empty() && !u.detour.empty()) u.route_goal={-1,-1};
                u.detour=std::move(best); u.blocked_ticks=0;
            }
        }
        if (u.order == Order::Move && u.queue.empty() && u.x == u.goal_x && u.z == u.goal_z) u.order = Order::Stop;
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
            if (u.queue.empty() && u.x==u.goal_x && u.z==u.goal_z) u.order=Order::Stop;
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
            b.production_queue=0; b.queue_lancers=0; b.production_ticks=0; b.spawn_blocked=false;
            destroyed=true;
        }
    }
    if (destroyed) rebuild_navigation();
    update_vision();
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
        add(static_cast<uint8_t>(u.kind)); add(u.cargo); add(u.cargo_kind); add(u.resource_id); add(u.build_id); add(u.work_ticks); add(u.returning);
        add(u.id); add(u.player); add(u.x); add(u.z); add(u.hp); add(static_cast<uint8_t>(u.order));
        add(u.target_id); add(u.target_structure_id); add(u.cooldown); add(u.moving); add(u.goal_x); add(u.goal_z); add(u.next_x); add(u.next_z);
        add(u.route_goal.x); add(u.route_goal.z); add(u.path.size());
        for (const auto& point : u.path) { add(point.x); add(point.z); }
        add(u.blocked_ticks); add(u.detour.size());
        for (const auto& point : u.detour) { add(point.x); add(point.z); }
        if (!u.queue.empty()) { add(u.queue.size()); for (const auto& leg : u.queue) { add(static_cast<uint8_t>(leg.order)); add(leg.x); add(leg.z); } }
    }
    for (int p=0;p<2;++p) { add(salvage_[p]); add(flux_[p]); add(research_ticks_[p]); add(researched_[p]); add(static_cast<uint8_t>(results_[p])); add(result_sequences_[p]); }
    add(structures_.size());
    for (const auto& b:structures_) { add(b.id); add(b.player); add(static_cast<uint8_t>(b.kind)); add(b.x); add(b.z); add(b.hp); add(b.build_ticks);
        add(b.production_queue); add(b.queue_lancers); add(b.production_ticks); add(b.spawn_blocked); }
    add(deposits_.size());
    for (const auto& d:deposits_) { add(d.id); add(d.x); add(d.z); add(d.remaining); add(d.kind); add(d.depletion_seen_by); }
    for (const auto& player : vision_) for (auto cell : player) add(cell);
    if (map_==Map::Economy) for (uint8_t p=0;p<2;++p) {
        add(salvage_guard_cell_[p].x); add(salvage_guard_cell_[p].z); add(salvage_guard_seen_tick_[p]);
    }
    for (const auto& retreat:mining_retreats_) {
        add(retreat.worker); add(retreat.guard); add(retreat.slot.x); add(retreat.slot.z);
        add(retreat.post.x); add(retreat.post.z); add(retreat.range); add(retreat.last_seen);
    }
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

namespace {
struct ArmedContact {
    nav::Point position;
    int range;
};
nav::Point observed_service_slot(const Sim& sim,const Unit& miner,const std::array<nav::Point,8>& slots) {
    const nav::Point goal{miner.goal_x,miner.goal_z};
    if (std::find(slots.begin(),slots.end(),goal)!=slots.end()) return goal;
    for (const auto slot:slots) {
        if (std::none_of(sim.units().begin(),sim.units().end(),[&](const Unit& u) {
            return u.hp>0 && u.id!=miner.id && sim.visibility(miner.player,u.x/kScale,u.z/kScale)==2 &&
                std::abs(u.x-slot.x)<2*unit_radius && std::abs(u.z-slot.z)<2*unit_radius;
        })) return slot;
    }
    return slots.front();
}
int acquisition_radius(const Unit& unit) {
    return (unit.kind==UnitKind::Lancer?kLancerAcquireTiles:strider_acquire_tiles)*kScale;
}
bool within_worker_sight(nav::Point position,nav::Point slot) {
    const int64_t dx=slot.x/kScale*kScale+kScale/2-position.x,dz=slot.z/kScale*kScale+kScale/2-position.z;
    return dx*dx+dz*dz<=int64_t(worker_sight_tiles*kScale)*(worker_sight_tiles*kScale);
}
bool crosses_acquisition(nav::Point from,nav::Point to,const ArmedContact& contact) {
    const int64_t dx=to.x-from.x,dz=to.z-from.z;
    const int64_t x=contact.position.x-from.x,z=contact.position.z-from.z;
    const int64_t length2=dx*dx+dz*dz, projection=x*dx+z*dz;
    const int64_t radius2=int64_t(contact.range)*contact.range;
    if (projection<=0) return x*x+z*z<=radius2;
    if (projection>=length2) return (x-dx)*(x-dx)+(z-dz)*(z-dz)<=radius2;
    const int64_t cross=x*dz-z*dx;
    return cross*cross<=radius2*length2;
}
// A scout sees the defender before Hold and pursuit look different. Use
// only map geometry, its own orders and currently observed contacts.
nav::Point salvage_flank(const Sim& sim,const Unit& miner,nav::Point patch,nav::Point home,bool inbound) {
    const nav::Point start{miner.x,miner.z},destination=inbound?patch:home;
    std::vector<ArmedContact> contacts;
    for (const auto& enemy:sim.units()) {
        if (enemy.player==miner.player || enemy.hp<=0 || enemy.kind==UnitKind::Worker ||
            sim.visibility(miner.player,enemy.x/kScale,enemy.z/kScale)!=2) continue;
        contacts.push_back({{enemy.x,enemy.z},acquisition_radius(enemy)});
    }
    if (contacts.empty()) return {-1,-1};

    // The center of the field may be safe while the worker's slot is exposed.
    // Before Gather assigns a goal, estimate the slot using only observed units.
    const auto slots=sim.service_slots(miner,destination,inbound?kScale/2:kScale);
    auto end=observed_service_slot(sim,miner,slots);
    const auto dangerous=[&](nav::Point a,nav::Point b) {
        return std::any_of(contacts.begin(),contacts.end(),[&](const ArmedContact& c) {return crosses_acquisition(a,b,c);});
    };
    // Approach a safe side to inspect the field when the preferred slot is
    // still unseen. Gather will assign the actual slot once the miner arrives.
    if (inbound && dangerous(end,end) && !within_worker_sight(start,end)) {
        const auto safe=std::find_if(slots.begin(),slots.end(),[&](nav::Point slot) {
            return !dangerous(slot,slot);
        });
        if (safe!=slots.end()) end=*safe;
    }
    if (!dangerous(start,end) || dangerous(start,start) || dangerous(end,end)) return {-1,-1};

    const auto distance_sq=[](nav::Point a,nav::Point b) {
        const int64_t dx=a.x-b.x,dz=a.z-b.z;
        return dx*dx+dz*dz;
    };
    auto obstacles=map_terrain(sim.map());
    const auto add=[&](int x,int z,int size) { obstacles.push_back({x-size,z-size,x+size,z+size}); };
    // The opposite field has a public map location even before the first visit.
    add(patch.x,patch.z,kScale/2);
    for (const auto& d:sim.deposits()) if ((d.x!=patch.x || d.z!=patch.z) && sim.visibility(miner.player,d.x/kScale,d.z/kScale)==2)
        add(d.x,d.z,kScale/2);
    for (const auto& b:sim.structures()) if (b.hp>0 && sim.visibility(miner.player,b.x/kScale,b.z/kScale)==2)
        add(b.x,b.z,kScale);
    const auto clear=[&](nav::Point a,nav::Point b,int clearance) {
        return std::none_of(obstacles.begin(),obstacles.end(),[&](nav::Rect r) {
            return !nav::segment_clear(a,b,{r.min_x-clearance,r.min_z-clearance,r.max_x+clearance,r.max_z+clearance});
        });
    };
    std::optional<nav::World> known_world;
    const auto safe_leg=[&](nav::Point from,nav::Point to) {
        if (clear(from,to,unit_radius)) {
            return clear(from,to,unit_radius+movement_speed/2) && !dangerous(from,to);
        }
        // A leg straight through the middle of a blocker has two equivalent
        // ways around it. Navigation can break that tie on different sides
        // after rotating the map; choose a waypoint that specifies a side.
        for (const auto r:obstacles) {
            const bool through_x=from.z==to.z && 2*from.z==r.min_z+r.max_z &&
                std::min(from.x,to.x)<r.min_x && std::max(from.x,to.x)>r.max_x;
            const bool through_z=from.x==to.x && 2*from.x==r.min_x+r.max_x &&
                std::min(from.z,to.z)<r.min_z && std::max(from.z,to.z)>r.max_z;
            if (through_x || through_z) return false;
        }
        if (!known_world) known_world.emplace(terrain_bounds(sim.map()),obstacles,unit_radius);
        const auto route=known_world->route(from,to);
        for (auto turn:route) {
            if (dangerous(from,turn)) return false;
            from=turn;
        }
        return from==to;
    };
    const auto useful=[&](nav::Point point) {
        return point.x>=kScale+unit_radius && point.z>=kScale+unit_radius &&
            point.x<=(sim.width()-1)*kScale-unit_radius && point.z<=(sim.height()-1)*kScale-unit_radius &&
            distance_sq(point,patch)<=int64_t(7*kScale)*(7*kScale) && distance_sq(point,end)<distance_sq(start,end) &&
            !dangerous(point,point) && safe_leg(start,point) && safe_leg(point,end);
    };
    for (const auto& contact:contacts) {
        if (!crosses_acquisition(start,end,contact)) continue;
        const auto pos=contact.position;
        const int reach=contact.range;
        const int away_x=(patch.x>pos.x)-(patch.x<pos.x);
        const int away_z=(patch.z>pos.z)-(patch.z<pos.z);
        const nav::Point x_side{pos.x+(home.x<pos.x?-reach:reach),patch.z+away_z*kScale};
        const nav::Point z_side{patch.x+away_x*kScale,pos.z+(home.z<pos.z?-reach:reach)};
        if (useful(x_side)) return x_side;
        if (useful(z_side)) return z_side;
    }

    // An offset defender can put both axis flanks out of sight of the field.
    // Check closer detours first, with ties ordered in the miner's orientation.
    struct FlankCandidate { uint64_t length; nav::Point point; };
    std::vector<FlankCandidate> candidates;
    const int sign=miner.player==0?1:-1;
    for (int x=-7;x<=7;++x) for (int z=-7;z<=7;++z) {
        // Sweep the two lanes beside each axis of the deposit. A point one
        // tile off its center line clears the expanded half-tile footprint.
        if (std::abs(x)!=1 && std::abs(z)!=1) continue;
        if (x*x+z*z>49) continue;
        const nav::Point point{patch.x+sign*x*kScale,patch.z+sign*z*kScale};
        const auto cost=nav::ceil_sqrt(distance_sq(start,point))+nav::ceil_sqrt(distance_sq(point,end));
        candidates.push_back({cost,point});
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const FlankCandidate& a,const FlankCandidate& b) {
        return a.length<b.length;
    });
    for (const auto& candidate:candidates) if (useful(candidate.point)) return candidate.point;
    return {-1,-1};
}
bool guards_edge_slot(const Sim& sim,const Unit& enemy,uint8_t player,nav::Point patch,nav::Point slot) {
    if (enemy.player==player || enemy.hp<=0 || enemy.kind==UnitKind::Worker ||
        sim.visibility(player,enemy.x/kScale,enemy.z/kScale)!=2) return false;
    const ArmedContact contact{{enemy.x,enemy.z},acquisition_radius(enemy)};
    // A defender outside the field itself can cover just its near perimeter.
    const int64_t dx=patch.x-enemy.x,dz=patch.z-enemy.z;
    return dx*dx+dz*dz>=int64_t(contact.range)*contact.range && crosses_acquisition(slot,slot,contact);
}
// When a new miner could be funded, inspect the reserve before leaving. Look
// at most one tile farther along the current navigation leg; stay outside the
// guard's acquisition with both the post and field in sight.
bool can_survey_on_flank(const Unit& worker,nav::Point patch,const Unit& guard) {
    const nav::Point start{worker.x,worker.z},post{guard.x,guard.z};
    const ArmedContact contact{post,acquisition_radius(guard)};
    const int64_t dx=worker.next_x-start.x,dz=worker.next_z-start.z;
    const int length=static_cast<int>(nav::ceil_sqrt(dx*dx+dz*dz));
    for (int d=movement_speed/2;d<=std::min(length,kScale);d+=movement_speed/2) {
        const nav::Point point{start.x+static_cast<int>(d*dx/length),start.z+static_cast<int>(d*dz/length)};
        if (within_worker_sight(point,patch) && within_worker_sight(point,post) &&
            !crosses_acquisition(start,point,contact)) return true;
    }
    return false;
}
}

bool Sim::waiting_for_mining_slot(uint32_t worker) const {
    return std::any_of(mining_retreats_.begin(),mining_retreats_.end(),[&](const MiningRetreat& r) {return r.worker==worker;});
}

void Sim::remember_mining_retreat(const Unit& miner) {
    if (miner.hp<=0 || miner.kind!=UnitKind::Worker || miner.cargo || waiting_for_mining_slot(miner.id)) return;
    const auto patch=home_salvage(1-miner.player);
    const auto slot=observed_service_slot(*this,miner,service_slots(miner,patch,kScale/2));
    if (!within_worker_sight({miner.x,miner.z},slot)) return;
    for (const auto& guard:units_) if (guards_edge_slot(*this,guard,miner.player,patch,slot))
        mining_retreats_.push_back({miner.id,guard.id,slot,{guard.x,guard.z},acquisition_radius(guard),tick_});
}

void Sim::update_mining_retreats() {
    for (auto& r:mining_retreats_) {
        const auto& worker=units_[r.worker-1];
        const auto& guard=units_[r.guard-1];
        bool cleared=worker.hp<=0;
        if (guard.hp>0 && visibility(worker.player,guard.x/kScale,guard.z/kScale)==2) {
            r.post={guard.x,guard.z};
            r.last_seen=tick_;
            cleared=cleared || !crosses_acquisition(r.slot,r.slot,{r.post,r.range});
        } else {
            cleared=cleared || visibility(worker.player,r.post.x/kScale,r.post.z/kScale)==2 ||
                tick_-r.last_seen>=30*kTicksPerSecond;
        }
        if (!cleared && worker.order!=Order::ReturnCargo && tick_%kTicksPerSecond==0 &&
            visibility(worker.player,r.slot.x/kScale,r.slot.z/kScale)==2) {
            // A parked unit can make Gather assign a different, safe slot.
            const auto patch=home_salvage(1-worker.player);
            const auto next=observed_service_slot(*this,worker,service_slots(worker,patch,kScale/2));
            cleared=next!=r.slot && visibility(worker.player,next.x/kScale,next.z/kScale)==2 &&
                !crosses_acquisition(next,next,{r.post,r.range});
        }
        if (cleared) r.worker=0;
    }
    std::erase_if(mining_retreats_,[](const MiningRetreat& r) {return !r.worker;});
}

std::vector<Command> make_ai_commands(const Sim& sim, uint8_t player, uint32_t& sequence) {
    if (player >= 2 || sim.winner() != -1) return {};
    const bool decision=sim.tick()%kTicksPerSecond==0;
    if (!decision && (sim.map()!=Map::Economy || !sim.knows_depleted(player,sim.deposits()[player]))) return {};
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
        const auto alternate=home_salvage(1-player);
        const int32_t approach_z=alternate.z+(player==0?-3:3)*kScale;
        const bool home_depleted=sim.knows_depleted(player,sim.deposits()[player]);
        const auto guard=sim.last_salvage_guard_cell(player);
        const bool guarded=guard.x>=0;
        std::vector<uint32_t> routing;
        for (const auto& u:sim.units()) {
            if (!home_depleted || guarded) break;
            if (u.player!=player || u.hp<=0 || u.kind!=UnitKind::Worker) continue;
            if (sim.waiting_for_mining_slot(u.id)) continue;
            const bool collecting=u.order==Order::Gather && u.resource_id==sim.deposits()[1-player].id;
            const bool scouting=u.order==Order::Move && u.goal_x==alternate.x && u.goal_z==approach_z;
            const int dx=u.goal_x-alternate.x,dz=u.goal_z-alternate.z;
            const bool flanking=u.order==Order::Move && !scouting && dx*dx+dz*dz<=7*kScale*7*kScale &&
                u.blocked_ticks<kTicksPerSecond;
            if (!decision) {
                // Between regular economy decisions a newly sighted contact
                // can cross acquisition range. Check active approaches
                // locally, and check the instant a full or final load turns home.
                const int64_t x=u.x-alternate.x,z=u.z-alternate.z;
                constexpr int local_range=(7+kLancerAcquireTiles)*kScale;
                const bool approaching=(scouting || (collecting && !u.returning && !u.cargo)) &&
                    x*x+z*z<=int64_t(local_range)*local_range;
                const bool leaving=collecting && u.returning && u.cargo && u.goal_x==u.x && u.goal_z==u.z;
                if (!approaching && !leaving && !flanking) continue;
            }
            if (!u.cargo && (flanking || scouting || (collecting && !u.returning) || u.order==Order::Hold)) {
                const auto slot=observed_service_slot(sim,u,sim.service_slots(u,alternate,kScale/2));
                if (within_worker_sight({u.x,u.z},slot)) {
                    const auto guard=std::find_if(sim.units().begin(),sim.units().end(),[&](const Unit& enemy) {
                        return guards_edge_slot(sim,enemy,player,alternate,slot);
                    });
                    if (guard!=sim.units().end()) {
                        const bool can_recruit=flanking && sim.salvage(player)>=kWorkerCost &&
                            std::count_if(sim.units().begin(),sim.units().end(),[&](const Unit& other) {
                                return other.hp>0 && other.player==player && other.kind==UnitKind::Worker;
                            })+anchor->production_queue<3;
                        const bool sees_stock=sim.visibility(player,alternate.x/kScale,alternate.z/kScale)==2;
                        if (can_recruit && sees_stock && !decision) emit(Order::Hold,{u.id});
                        else if (!(can_recruit && !sees_stock && can_survey_on_flank(u,alternate,*guard)))
                            emit(Order::ReturnCargo,{u.id});
                        routing.push_back(u.id);
                        continue;
                    }
                }
            }
            if (flanking) {
                // Finish a safe flank even if the contact goes dark along it.
                routing.push_back(u.id);
                continue;
            }
            if (!scouting && !collecting && !(u.order==Order::ReturnCargo && u.cargo)) continue;
            if (collecting && !u.returning && u.cargo) continue;
            const auto flank=salvage_flank(sim,u,alternate,{anchor->x,anchor->z},!u.returning);
            if (flank.x>=0 && !(u.order==Order::Move && u.goal_x==flank.x && u.goal_z==flank.z)) {
                emit(Order::Move,{u.id},flank.x,flank.z);
                routing.push_back(u.id);
            }
        }
        if (!decision) return commands;
        const auto routing_worker=[&](const Unit& u) {
            return std::find(routing.begin(),routing.end(),u.id)!=routing.end();
        };
        const auto needs_recall=[&](const Unit& u) {
            if (!guarded) return false;
            if (u.order==Order::Gather && u.resource_id==sim.deposits()[1-player].id) return true;
            if (u.order==Order::Move && u.goal_x==alternate.x && u.goal_z==approach_z) return true;
            const int64_t dx=u.x-(guard.x*kScale+kScale/2),dz=u.z-(guard.z*kScale+kScale/2);
            return dx*dx+dz*dz<=int64_t(9*kScale)*(9*kScale);
        };
        const auto reserved_for_recall=[&](const Unit& u) {
            return routing_worker(u) || (home_depleted && u.order==Order::ReturnCargo &&
                (guarded || u.cargo>0 || sim.waiting_for_mining_slot(u.id))) || needs_recall(u);
        };
        const Unit* builder=nullptr;
        bool constructing=false;
        uint32_t worker_count=0;
        bool army_can_scout=foundry && foundry->production_queue>0 && !foundry->spawn_blocked;
        for (const auto& u:sim.units()) if (u.hp>0 && u.player==player) {
            if (u.kind!=UnitKind::Worker) { army_can_scout=true; continue; }
            ++worker_count;
            if (!builder && !reserved_for_recall(u)) builder=&u;
            if (foundry && u.order==Order::Build && u.build_id==foundry->id) constructing=true;
        }
        // The only alternate salvage on this map lies at the opposite start.
        // Its position is public. Its reserve must be observed by this team.
        bool alternate_depleted=false;
        std::vector<const Deposit*> visible_salvage;
        int32_t salvage_to_mine=0;
        for (const auto& d:sim.deposits()) {
            if (d.x==alternate.x && d.z==alternate.z) {
                alternate_depleted=sim.knows_depleted(player,d);
                if (guarded) continue;
            }
            if (sim.visibility(player,d.x/kScale,d.z/kScale)==2 && d.kind==0 && d.remaining>0) {
                visible_salvage.push_back(&d);
                salvage_to_mine+=d.remaining;
            }
        }
        const bool needs_scout=visible_salvage.empty() && !alternate_depleted && !guarded;
        // Preserve one worker's cost for the army's sighting. If neither an
        // army nor workers can scout, use one recruit to look for new income.
        const uint32_t workforce=worker_count+anchor->production_queue;
        // An existing miner can finish a small remnant. Add each further
        // worker only if another worker cost remains in observed salvage.
        uint32_t target_workers=needs_scout && !army_can_scout?1u:0u;
        if (salvage_to_mine>0) target_workers=std::min(3,1+salvage_to_mine/kWorkerCost);
        int32_t available_salvage=sim.salvage(player);
        const bool foundry_can_train=foundry && foundry->build_ticks>=kBuildTicks && foundry->production_queue<kProductionQueueLimit;
        // The anchor sees the home patch. Once the alternate is accounted
        // for too, keep the military purchase if a first miner could never fund it.
        const bool all_salvage_known=alternate_depleted || sim.visibility(player,alternate.x/kScale,alternate.z/kScale)==2;
        if (!workforce && all_salvage_known && foundry_can_train && !foundry->spawn_blocked &&
            available_salvage>=kStriderCost && available_salvage-kWorkerCost+salvage_to_mine<kStriderCost)
            target_workers=0;
        if (needs_scout && !workforce && army_can_scout) available_salvage-=std::min(available_salvage,kWorkerCost);
        const uint32_t reserved=sim.population_reserved(player);
        uint32_t population=sim.population_used(player)+reserved;
        size_t roster=sim.units().size()+reserved+sim.population_reserved(1-player);
        if (workforce<target_workers && available_salvage>=kWorkerCost &&
            anchor->production_queue<kProductionQueueLimit && population<kPopulationCap && roster<kLifetimeUnitLimit) {
            emit(Order::TrainWorker,{anchor->id});
            available_salvage-=kWorkerCost;
            ++population;
            ++roster;
        }
        uint32_t assigned_builder=0;
        if (builder && foundry && foundry->build_ticks<kBuildTicks && !constructing) {
            emit(Order::Build,{builder->id},foundry->x,foundry->z); assigned_builder=builder->id;
        } else if (builder && !foundry && available_salvage>=kFoundryCost) {
            // Prefer a forward site away from the mining lane. Enumerate a
            // bounded anchor-linked fallback if units or ruins occupy that site.
            const int direction=player==0?1:-1;
            bool placed=false;
            for (int z_offset : {3,4,5,0,-3,-4,-5}) {
                for (int x_offset : {5,4,3,0,-3,-4,-5}) {
                    const int x=anchor->x+direction*x_offset*kScale,z=anchor->z+direction*z_offset*kScale;
                    if (!sim.can_build(player,x,z)) continue;
                    emit(Order::Build,{builder->id},x,z); assigned_builder=builder->id; placed=true; break;
                }
                if (placed) break;
            }
        }
        const Unit* scout=nullptr;
        if (needs_scout && routing.empty()) {
            for (const auto& u:sim.units()) {
                if (u.hp<=0 || u.player!=player || u.kind!=UnitKind::Worker || u.id==assigned_builder ||
                    u.order==Order::Build || u.order==Order::ReturnCargo || u.returning || u.cargo>0 ||
                    sim.waiting_for_mining_slot(u.id)) continue;
                // A worker already on a valid salvage trip provides its own sight.
                if (u.order==Order::Gather && u.resource_id && sim.deposits()[u.resource_id-1].kind==0) continue;
                if (!scout) scout=&u;
                if (u.order==Order::Move && u.goal_x==alternate.x && u.goal_z==approach_z) {
                    scout=&u; break;
                }
            }
            if (scout && (scout->order!=Order::Move || scout->goal_x!=alternate.x || scout->goal_z!=approach_z))
                emit(Order::Move,{scout->id},alternate.x,approach_z);
        }
        // Once the Foundry stands, dedicate one worker to visible flux only when
        // another worker remains for salvage. The last survivor must retain the
        // common-resource income needed to replace an army after casualties.
        uint32_t flux_worker=0;
        bool flux_mined=false;
        const Deposit* flux_site=nullptr;
        if (foundry && foundry->build_ticks>=kBuildTicks) {
            for (const auto& u:sim.units()) if (u.hp>0 && u.player==player && u.kind==UnitKind::Worker && u.order==Order::Gather &&
                u.resource_id && sim.deposits()[u.resource_id-1].kind==1 && !sim.knows_depleted(player,sim.deposits()[u.resource_id-1])) flux_mined=true;
            int64_t best=std::numeric_limits<int64_t>::max();
            for (const auto& d:sim.deposits()) if (sim.visibility(player,d.x/kScale,d.z/kScale)==2 && d.kind==1 && d.remaining>0) {
                const int64_t dx=int64_t(anchor->x)-d.x,dz=int64_t(anchor->z)-d.z;
                if (dx*dx+dz*dz<best) { best=dx*dx+dz*dz; flux_site=&d; }
            }
            if (worker_count>1 && !flux_mined && flux_site) for (const auto& u:sim.units())
                if (u.hp>0 && u.player==player && u.kind==UnitKind::Worker && &u!=scout && u.id!=assigned_builder && u.order!=Order::Build &&
                    !reserved_for_recall(u)) {
                    flux_worker=u.id; emit(Order::Gather,{u.id},flux_site->x,flux_site->z); break;
                }
            if (!sim.researched(player) && !sim.research_ticks(player) && sim.flux(player)>=kResearchFluxCost)
                emit(Order::Research,{foundry->id});
        }
        // An empty scout can turn back immediately when its escort clears the
        // site in view. A worker carrying ore finishes its return to the bank.
        const bool alternate_open=home_depleted && !guarded && !visible_salvage.empty();
        for (const auto& u:sim.units()) if (u.hp>0 && u.player==player && u.kind==UnitKind::Worker &&
            &u!=scout && !routing_worker(u) && !sim.waiting_for_mining_slot(u.id) && u.id!=assigned_builder && u.id!=flux_worker && u.order!=Order::Build &&
            (u.order!=Order::ReturnCargo || (alternate_open && !u.cargo))) {
            if (needs_recall(u)) {
                emit(Order::ReturnCargo,{u.id});
                continue;
            }
            // Reassign an existing sole flux miner once; Gather's mixed-cargo
            // rule delivers its flux first, then resumes the salvage deposit.
            // Existing salvage trips keep their goals and work progress.
            const bool sole_flux_miner=worker_count==1 && u.resource_id>0 &&
                u.resource_id<=sim.deposits().size() && sim.deposits()[u.resource_id-1].kind==1;
            if (u.order==Order::Gather && !sole_flux_miner) continue;
            const Deposit* nearest=nullptr;
            int64_t distance=std::numeric_limits<int64_t>::max();
            for (const auto* d:visible_salvage) {
                const int64_t dx=int64_t(u.x)-d->x,dz=int64_t(u.z)-d->z;
                if (dx*dx+dz*dz<distance) { distance=dx*dx+dz*dz; nearest=d; }
            }
            if (nearest) emit(Order::Gather,{u.id},nearest->x,nearest->z);
            else if (u.cargo>0) emit(Order::ReturnCargo,{u.id});
        }
        if (foundry_can_train && available_salvage>=kStriderCost && population<kPopulationCap && roster<kLifetimeUnitLimit)
        {
            // After Hardened Plating, keep roughly one Lancer behind every two Striders when flux allows.
            int striders=0,lancers=0;
            for (const auto& u:sim.units()) if (u.hp>0 && u.player==player) {
                if (u.kind==UnitKind::Strider) ++striders; else if (u.kind==UnitKind::Lancer) ++lancers;
            }
            const bool want_lancer=sim.researched(player) && sim.flux(player)>=kLancerFluxCost && available_salvage>=kLancerCost && lancers*2<=striders;
            emit(want_lancer?Order::TrainLancer:Order::TrainStrider,{foundry->id});
        }
        std::vector<uint32_t> army;
        for (const auto& u:sim.units()) if (u.hp>0 && u.player==player && u.kind!=UnitKind::Worker && u.order!=Order::AttackMove)
            army.push_back(u.id);
        if (!army.empty()) emit(Order::AttackMove,std::move(army),(player==0?59:4)*kScale+128,(player==0?35:12)*kScale+128);
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
