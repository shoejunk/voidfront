#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace vf {
inline constexpr int kScale = 256, kTicksPerSecond = 20;
inline constexpr int kMapWidth = 32, kMapHeight = 24;
inline constexpr uint32_t kProtocolVersion = 1;
enum class Order : uint8_t { Stop, Move, AttackMove, Hold };
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
    uint16_t cooldown = 0;
    bool moving = false;
    // Goal and reserved destination are authoritative, included in hashes.
    int32_t goal_x = 0, goal_z = 0, next_x = 0, next_z = 0;
};
class Sim {
public:
    explicit Sim(uint32_t seed = 1, uint32_t units_per_team = 6);
    bool submit(Command command);
    void step();
    uint32_t tick() const { return tick_; }
    const std::vector<Unit>& units() const { return units_; }
    bool blocked(int x, int z) const;
    uint64_t hash() const;
    // -1 ongoing, 0/1 winning player, 2 draw.
    int winner() const;
private:
    uint32_t tick_ = 0, rng_ = 1;
    std::array<uint32_t, 2> last_sequence_{};
    std::vector<Unit> units_;
    std::vector<Command> pending_;
    void apply(const Command& command);
    int route(const Unit& unit, int goal) const;
};
std::vector<uint8_t> serialize_command(const Command& command);
bool deserialize_command(std::span<const uint8_t> bytes, Command& command);
std::vector<Command> make_ai_commands(const Sim& sim, uint8_t player, uint32_t& sequence);
}
