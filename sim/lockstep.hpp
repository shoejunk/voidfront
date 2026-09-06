#pragma once
#include "voidfront_sim.hpp"
#include <map>
#include <optional>

namespace vf {
inline constexpr uint32_t kLockstepProtocolVersion = 2;
#ifdef VF_CONTENT_ID
inline constexpr uint64_t kLockstepContentId = VF_CONTENT_ID;
#else
// Standalone fallback. Production builds provide the simulation source fingerprint.
inline constexpr uint64_t kLockstepContentId = 0x56464c5300000001ull;
#endif
inline constexpr uint32_t kMaxFrameCommands = 16;
inline constexpr uint32_t kMaxFutureTicks = 64;
inline constexpr size_t kMaxFrameBytes = 25 + kMaxFrameCommands * (4 + 26 + 256 * 4);

struct TickFrame {
    uint32_t tick = 0;
    uint8_t player = 0;
    std::vector<Command> commands;
};
// Little-endian integers, explicit lengths, exact canonical encoding; no trailing data.
std::vector<uint8_t> serialize_frame(const TickFrame& frame);
bool deserialize_frame(std::span<const uint8_t> bytes, TickFrame& frame);
enum class ReceiveResult { Accepted, Duplicate, Invalid, Conflict, TooFar, Stale };
enum class AdvanceResult { Waiting, Advanced, Invalid };
class Lockstep {
public:
    explicit Lockstep(uint32_t seed = 42, uint32_t units_per_team = 6);
    const Sim& sim() const { return sim_; }
    ReceiveResult receive(const TickFrame& frame);
    // Missing frames never advance. Invalid is terminal for this immutable turn.
    // Both players' commands validate on a copy before any authoritative state changes.
    AdvanceResult advance();
private:
    Sim sim_;
    std::map<uint32_t, std::array<std::optional<TickFrame>, 2>> frames_;
};
}
