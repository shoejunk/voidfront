#include "lockstep.hpp"
#include <algorithm>
#include <limits>
#include <utility>

namespace vf {
namespace {
void append(std::vector<uint8_t>& bytes, uint64_t value, size_t count) {
    for (size_t i = 0; i < count; ++i) bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
}
bool canonical(const TickFrame& frame) {
    if (frame.player >= 2 || frame.tick == std::numeric_limits<uint32_t>::max() ||
        frame.commands.size() > kMaxFrameCommands) return false;
    uint32_t sequence = 0;
    for (const auto& c : frame.commands) {
        if (c.player != frame.player || c.tick != frame.tick || c.sequence <= sequence ||
            serialize_command(c).empty()) return false;
        sequence = c.sequence;
    }
    return true;
}
}

std::vector<uint8_t> serialize_frame(const TickFrame& frame) {
    if (!canonical(frame)) return {};
    std::vector<uint8_t> bytes{'V', 'F', 'L', 1};
    append(bytes, kLockstepProtocolVersion, 4);
    append(bytes, kLockstepContentId, 8);
    append(bytes, frame.tick, 4);
    bytes.push_back(frame.player);
    append(bytes, frame.previous_hash, 8);
    append(bytes, frame.commands.size(), 4);
    for (const auto& c : frame.commands) {
        const auto command = serialize_command(c);
        append(bytes, command.size(), 4);
        bytes.insert(bytes.end(), command.begin(), command.end());
    }
    return bytes;
}

bool deserialize_frame(std::span<const uint8_t> bytes, TickFrame& out) {
    if (bytes.size() < 33 || bytes.size() > kMaxFrameBytes || bytes[0] != 'V' ||
        bytes[1] != 'F' || bytes[2] != 'L' || bytes[3] != 1) return false;
    size_t pos = 4;
    const auto read = [&](size_t count) {
        uint64_t value = 0;
        for (size_t i = 0; i < count; ++i) value |= uint64_t(bytes[pos++]) << (8 * i);
        return value;
    };
    if (read(4) != kLockstepProtocolVersion || read(8) != kLockstepContentId) return false;
    TickFrame frame;
    frame.tick = static_cast<uint32_t>(read(4));
    frame.player = static_cast<uint8_t>(read(1));
    frame.previous_hash = read(8);
    const auto count = read(4);
    if (count > kMaxFrameCommands) return false;
    for (uint64_t i = 0; i < count; ++i) {
        if (bytes.size() - pos < 4) return false;
        const auto length = read(4);
        if (length > bytes.size() - pos) return false;
        Command command;
        if (!deserialize_command(bytes.subspan(pos, static_cast<size_t>(length)), command)) return false;
        pos += static_cast<size_t>(length);
        frame.commands.push_back(std::move(command));
    }
    if (pos != bytes.size() || !canonical(frame)) return false;
    out = std::move(frame);
    return true;
}

Lockstep::Lockstep(uint32_t seed, uint32_t units_per_team) : sim_(seed, units_per_team) {}

ReceiveResult Lockstep::receive(const TickFrame& frame) {
    if (!canonical(frame)) return ReceiveResult::Invalid;
    if (frame.tick < sim_.tick()) return ReceiveResult::Stale;
    if (frame.tick - sim_.tick() > kMaxFutureTicks) return ReceiveResult::TooFar;
    for (const auto& c : frame.commands) for (const auto id : c.units) {
        if (id > sim_.units().size() || sim_.units()[id - 1].player != frame.player)
            return ReceiveResult::Invalid;
    }
    auto& slot = frames_[frame.tick][frame.player];
    if (slot) return serialize_frame(*slot) == serialize_frame(frame) ? ReceiveResult::Duplicate : ReceiveResult::Conflict;
    slot = frame;
    return ReceiveResult::Accepted;
}

AdvanceResult Lockstep::advance() {
    const auto found = frames_.find(sim_.tick());
    if (found == frames_.end() || !found->second[0] || !found->second[1]) return AdvanceResult::Waiting;
    const auto prior = sim_.state_hash();
    for (const auto& frame : found->second)
        if (frame->previous_hash != prior) return AdvanceResult::Desync;
    Sim candidate = sim_;
    for (const auto& frame : found->second) for (const auto& command : frame->commands)
        if (!candidate.submit(command)) return AdvanceResult::Invalid;
    candidate.step();
    sim_ = std::move(candidate);
    frames_.erase(found);
    return AdvanceResult::Advanced;
}
}
