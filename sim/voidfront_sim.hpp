#pragma once
#include "navigation.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace vf {
inline constexpr int kScale = 256, kTicksPerSecond = 20;
inline constexpr int kMapWidth = 32, kMapHeight = 24;
inline constexpr int kMaxMapSize = 128;
inline constexpr uint32_t kProtocolVersion = 9;
enum class Map : uint32_t { Foundry = 0, Scale128 = 1, Economy = 2 };
int map_width(Map map);
int map_height(Map map);
const std::vector<nav::Rect>& map_terrain(Map map);
enum class Order : uint8_t { Stop, Move, AttackMove, Hold, Gather, ReturnCargo, Build, TrainStrider, CancelProduction };
enum class UnitKind : uint8_t { Strider, Worker };
enum class StructureKind : uint8_t { Anchor, Foundry };
enum class CommandResult : uint8_t { None, Accepted, InvalidTarget, InsufficientSalvage, InvalidPlacement, InvalidWorker,
    InvalidStructure, NotReady, QueueFull, PopulationFull, RosterFull, EmptyQueue };
inline constexpr int kFoundryCost=100, kBuildTicks=100, kCargoCapacity=10, kGatherTicks=10;
inline constexpr int kStriderCost=50;
inline constexpr uint32_t kProductionTicks=100, kProductionQueueLimit=5, kPopulationCap=12, kLifetimeUnitLimit=4096;
struct Structure {
    uint32_t id=0;
    uint8_t player=0;
    StructureKind kind=StructureKind::Anchor;
    int32_t x=0,z=0,hp=1000;
    uint32_t build_ticks=kBuildTicks;
    uint32_t production_queue=0,production_ticks=0;
    bool spawn_blocked=false;
};
struct Deposit { uint32_t id=0; int32_t x=0,z=0,remaining=2000; };
struct Command {
    uint32_t tick = 0, sequence = 0;
    uint8_t player = 0;
    Order order = Order::Stop;
    std::vector<uint32_t> units;
    int32_t x = 0, z = 0;
};
struct Unit {
    uint32_t id = 0;
    uint8_t player = 0;
    int32_t x = 0, z = 0, hp = 100;
    Order order = Order::Stop;
    uint32_t target_id = 0;
    uint32_t target_structure_id = 0;
    uint16_t cooldown = 0;
    bool moving = false;
    UnitKind kind=UnitKind::Strider;
    int32_t cargo=0;
    uint32_t resource_id=0,build_id=0,work_ticks=0;
    bool returning=false;
    // Goal, current waypoint and remaining route are authoritative, included in hashes.
    int32_t goal_x = 0, goal_z = 0, next_x = 0, next_z = 0;
    std::vector<nav::Point> path;
    nav::Point route_goal{};
    std::vector<nav::Point> detour;
    uint16_t blocked_ticks = 0;
};
class Sim {
public:
    explicit Sim(uint32_t seed = 1, uint32_t units_per_team = 6, Map map = Map::Foundry);
    bool submit(Command command);
    void step();
    uint32_t tick() const { return tick_; }
    const std::vector<Unit>& units() const { return units_; }
    Map map() const { return map_; }
    const std::vector<Structure>& structures() const { return structures_; }
    const std::vector<Deposit>& deposits() const { return deposits_; }
    int32_t salvage(uint8_t player) const { return player<2?salvage_[player]:0; }
    uint32_t population_used(uint8_t player) const;
    uint32_t population_reserved(uint8_t player) const;
    uint32_t population_cap(uint8_t player) const { return player<2 && map_==Map::Economy?kPopulationCap:0; }
    CommandResult command_result(uint8_t player) const { return player<2?results_[player]:CommandResult::None; }
    uint32_t result_sequence(uint8_t player) const { return player<2?result_sequences_[player]:0; }
    bool can_build(uint8_t player,int32_t x,int32_t z) const;
    int width() const { return map_width(map_); }
    int height() const { return map_height(map_); }
    bool blocked(int x, int z) const;
    // Cell visibility: 0 unexplored, 1 explored, 2 currently visible.
    uint8_t visibility(uint8_t player, int x, int z) const;
    const std::vector<uint8_t>& vision(uint8_t player) const { return vision_[player]; }
    uint64_t hash() const;
    // Executed authoritative state only; independent of future input arrival order.
    uint64_t state_hash() const;
    // -1 ongoing, 0/1 winning player, 2 draw.
    int winner() const;
private:
    std::array<std::vector<uint8_t>,2> vision_;
    void update_vision();
    Map map_;
    uint32_t tick_ = 0, rng_ = 1;
    std::array<uint32_t, 2> last_sequence_{};
    std::vector<Unit> units_;
    std::vector<Command> pending_;
    std::vector<Structure> structures_;
    std::vector<Deposit> deposits_;
    std::array<int32_t,2> salvage_{};
    std::array<CommandResult,2> results_{};
    std::array<uint32_t,2> result_sequences_{};
    void apply(const Command& command);
    void apply_economy(const Command& command);
    void economy_step();
    void apply_production(const Command& command);
    void production_step();
    void rebuild_navigation();
    void set_goal(Unit& unit,nav::Point goal);
    nav::Point service_point(const Unit& unit,int32_t x,int32_t z,int32_t extent,const nav::World* world=nullptr) const;
    nav::World navigation_;
};
std::vector<uint8_t> serialize_command(const Command& command);
bool deserialize_command(std::span<const uint8_t> bytes, Command& command);
std::vector<Command> make_ai_commands(const Sim& sim, uint8_t player, uint32_t& sequence);
}
