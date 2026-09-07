#include "voidfront_sim.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace vf {
namespace {
constexpr int cells = kMapWidth * kMapHeight;
int cell_of(int x, int z) { return z / kScale * kMapWidth + x / kScale; }
int center_x(int cell) { return cell % kMapWidth * kScale + kScale / 2; }
int center_z(int cell) { return cell / kMapWidth * kScale + kScale / 2; }
int manhattan(int a, int b) { return std::abs(a % kMapWidth - b % kMapWidth) + std::abs(a / kMapWidth - b / kMapWidth); }
const std::vector<nav::Rect>& terrain_rectangles() {
    static const std::vector<nav::Rect> result{{15*kScale,3*kScale,17*kScale,9*kScale},
        {15*kScale,16*kScale,17*kScale,21*kScale}};
    return result;
}
nav::Rect terrain_bounds() { return {kScale,kScale,(kMapWidth-1)*kScale,(kMapHeight-1)*kScale}; }
constexpr int unit_radius = 64, movement_speed = 32;
int64_t distance2(const Unit& a, const Unit& b) {
    const int64_t dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}
bool canonical(const Command& c) {
    return c.player < 2 && c.sequence > 0 && static_cast<uint8_t>(c.order) <= 3 &&
        !c.units.empty() && c.units.size() <= 256 && c.x >= 0 && c.z >= 0 &&
        c.x < kMapWidth * kScale && c.z < kMapHeight * kScale &&
        std::is_sorted(c.units.begin(), c.units.end()) &&
        std::adjacent_find(c.units.begin(), c.units.end()) == c.units.end() && c.units.front() > 0;
}
bool command_less(const Command& a, const Command& b) {
    return std::tie(a.tick, a.player, a.sequence) < std::tie(b.tick, b.player, b.sequence);
}
void append(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (i * 8)));
}
}

Sim::Sim(uint32_t seed, uint32_t count) : rng_(seed ? seed : 1), navigation_(terrain_bounds(), terrain_rectangles(), unit_radius) {
    if (count < 1 || count > 250) throw std::invalid_argument("units_per_team must be 1..250");
    for (uint8_t p = 0; p < 2; ++p) {
        for (uint32_t n = 0; n < count; ++n) {
            Unit u;
            u.id = static_cast<uint32_t>(units_.size()) + 1;
            u.player = p;
            const int x = p == 0 ? 2 + static_cast<int>(n / 20) : 29 - static_cast<int>(n / 20);
            const int z = count <= 12 ? 9 + static_cast<int>(n) : 2 + static_cast<int>(n % 20);
            u.x = u.goal_x = u.next_x = x * kScale + kScale / 2;
            u.z = u.goal_z = u.next_z = z * kScale + kScale / 2;
            rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
            u.cooldown = static_cast<uint16_t>(rng_ % 5);
            units_.push_back(u);
        }
    }
}

bool Sim::blocked(int x, int z) const {
    if (x <= 0 || z <= 0 || x >= kMapWidth - 1 || z >= kMapHeight - 1) return true;
    // Foundry ridges leave a broad central passage and two flanking corridors.
    return (x == 15 || x == 16) && ((z >= 3 && z <= 8) || (z >= 16 && z <= 20));
}

bool Sim::submit(Command command) {
    std::sort(command.units.begin(), command.units.end());
    if (!canonical(command) || command.tick < tick_ || command.tick - tick_ > 1200 ||
        command.sequence <= last_sequence_[command.player] || pending_.size() >= 4096) return false;
    for (const auto& c : pending_)
        if (c.player == command.player && c.sequence == command.sequence) return false;
    // A player's sequence order must agree with tick order even if packets arrive reordered.
    for (const auto& c : pending_)
        if (c.player == command.player && ((c.sequence < command.sequence && c.tick > command.tick) ||
            (c.sequence > command.sequence && c.tick < command.tick))) return false;
    for (const auto id : command.units)
        if (id > units_.size() || units_[id - 1].player != command.player) return false;
    pending_.push_back(std::move(command));
    std::sort(pending_.begin(), pending_.end(), command_less);
    return true;
}

void Sim::apply(const Command& c) {
    last_sequence_[c.player] = c.sequence;
    std::array<bool, cells> assigned{};
    const int requested = cell_of(c.x, c.z);
    for (const auto id : c.units) {
        auto& u = units_[id - 1];
        if (u.hp <= 0) continue;
        u.order = c.order;
        u.target_id = 0;
        u.path.clear(); u.next_x = u.x; u.next_z = u.z;
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
        int best = -1, score = std::numeric_limits<int>::max();
        for (int cell = 0; cell < cells; ++cell) {
            if (assigned[cell] || blocked(cell % kMapWidth, cell / kMapWidth)) continue;
            const int candidate = manhattan(cell, requested);
            if (candidate < score) { best = cell; score = candidate; }
        }
        if (best >= 0) {
            assigned[best] = true;
            u.goal_x = center_x(best); u.goal_z = center_z(best);
        }
    }
}

void Sim::step() {
    size_t applied = 0;
    while (applied < pending_.size() && pending_[applied].tick == tick_) apply(pending_[applied++]);
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(applied));
    std::vector<int32_t> damage(units_.size(), 0);
    std::vector<nav::Point> tick_start;
    for (const auto& u : units_) tick_start.push_back({u.x,u.z});
    constexpr int64_t attack_range2 = int64_t(3 * kScale) * (3 * kScale);
    constexpr int64_t acquire_range2 = int64_t(6 * kScale) * (6 * kScale);
    for (auto& u : units_) {
        u.moving = false; u.target_id = 0;
        if (u.hp <= 0) continue;
        if (u.cooldown > 0) --u.cooldown;
        const Unit* target = nullptr;
        int64_t nearest = acquire_range2 + 1;
        if (u.order != Order::Move) {
            for (const auto& other : units_) {
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
        if (u.order == Order::Stop || u.order == Order::Hold) continue;
        const nav::Point here{u.x,u.z};
        if (goal == here) { u.path.clear(); u.route_goal={-1,-1}; u.next_x=u.x; u.next_z=u.z; continue; }
        if (!(u.route_goal == goal)) {
            u.path = navigation_.route(here, goal); u.route_goal=goal;
        }
        while (!u.path.empty() && u.path.front() == here) u.path.erase(u.path.begin());
        if (u.path.empty()) { u.next_x=u.x; u.next_z=u.z; continue; }
        const auto waypoint = u.path.front();
        u.next_x=waypoint.x; u.next_z=waypoint.z;
        const int64_t dx = int64_t(waypoint.x)-u.x, dz = int64_t(waypoint.z)-u.z;
        const auto length = nav::ceil_sqrt(static_cast<uint64_t>(dx*dx+dz*dz));
        std::vector<nav::Point> candidates;
        if (length <= movement_speed) candidates.push_back(waypoint);
        else {
            // Nearby lattice samples cover both sides of the exact projected
            // step. A single component truncation can penetrate a tangent wall
            // and stall forever even when the visibility segment is clear.
            const int32_t step_x=static_cast<int32_t>(dx*movement_speed/static_cast<int64_t>(length));
            const int32_t step_z=static_cast<int32_t>(dz*movement_speed/static_cast<int64_t>(length));
            for (int x=-1;x<=1;++x) for (int z=-1;z<=1;++z) {
                const int64_t sx=step_x+x, sz=step_z+z;
                if (sx*sx+sz*sz<=movement_speed*movement_speed &&
                    (dx-sx)*(dx-sx)+(dz-sz)*(dz-sz)<dx*dx+dz*dz)
                    candidates.push_back({u.x+static_cast<int32_t>(sx),u.z+static_cast<int32_t>(sz)});
            }
            const auto error=[&](nav::Point p) {
                const int64_t ex=(int64_t(p.x)-u.x)*static_cast<int64_t>(length)-dx*movement_speed;
                const int64_t ez=(int64_t(p.z)-u.z)*static_cast<int64_t>(length)-dz*movement_speed;
                return ex*ex+ez*ez;
            };
            std::sort(candidates.begin(),candidates.end(),[&](nav::Point a,nav::Point b) {
                const auto ea=error(a),eb=error(b);
                return ea!=eb ? ea<eb : std::tie(a.x,a.z)<std::tie(b.x,b.z);
            });
        }
        for (const auto candidate : candidates) {
            bool free = navigation_.clear(here,candidate);
            for (const auto& other : units_) if (free && other.hp>0 && other.id!=u.id) {
                const auto old=tick_start[other.id-1];
                // Protect the entire other-unit sweep, including presentation interpolation.
                const nav::Rect occupied{std::min(old.x,other.x)-2*unit_radius,
                    std::min(old.z,other.z)-2*unit_radius,std::max(old.x,other.x)+2*unit_radius,
                    std::max(old.z,other.z)+2*unit_radius};
                free=nav::segment_clear(here,candidate,occupied);
            }
            if (free) { u.x=candidate.x; u.z=candidate.z; u.moving=!(candidate==here); break; }
        }
        if (u.order == Order::Move && u.x == u.goal_x && u.z == u.goal_z) u.order = Order::Stop;
    }
    for (size_t i = 0; i < units_.size(); ++i) {
        units_[i].hp = std::max(0, units_[i].hp - damage[i]);
        if (units_[i].hp == 0) units_[i].moving = false;
    }
    ++tick_;
}

int Sim::winner() const {
    std::array<bool, 2> live{};
    for (const auto& u : units_) if (u.hp > 0) live[u.player] = true;
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
    if (x >= kMapWidth * kScale || z >= kMapHeight * kScale || count == 0 || count > 256 || bytes.size() != 26 + count * 4) return false;
    c.x = static_cast<int32_t>(x); c.z = static_cast<int32_t>(z);
    for (uint32_t i = 0; i < count; ++i) c.units.push_back(read());
    if (!canonical(c)) return false;
    out = std::move(c); return true;
}

uint64_t Sim::state_hash() const {
    uint64_t h = 14695981039346656037ull;
    const auto add = [&](uint64_t v) { for (int i = 0; i < 8; ++i) { h ^= (v >> (8 * i)) & 255; h *= 1099511628211ull; } };
    add(kProtocolVersion); add(tick_); add(rng_); add(last_sequence_[0]); add(last_sequence_[1]); add(units_.size());
    for (const auto& u : units_) {
        add(u.id); add(u.player); add(u.x); add(u.z); add(u.hp); add(static_cast<uint8_t>(u.order));
        add(u.target_id); add(u.cooldown); add(u.moving); add(u.goal_x); add(u.goal_z); add(u.next_x); add(u.next_z);
        add(u.route_goal.x); add(u.route_goal.z); add(u.path.size());
        for (const auto& point : u.path) { add(point.x); add(point.z); }
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

std::vector<Command> make_ai_commands(const Sim& sim, uint8_t player, uint32_t& sequence) {
    if (player >= 2 || sim.winner() != -1 || sim.tick() % 20 != 0) return {};
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
