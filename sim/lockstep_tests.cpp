#include "lockstep.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace vf;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
Command command(uint32_t tick, uint8_t player, uint32_t sequence) {
    return {tick, sequence, player, Order::Move, {player == 0 ? 1u : 7u}, 12 * kScale + 128, 12 * kScale + 128};
}
TickFrame frame(const Sim& sim, uint8_t player, uint32_t sequence = 0) {
    TickFrame result{sim.tick(), player, sim.state_hash(), {}};
    if (sequence) result.commands.push_back(command(result.tick, player, sequence));
    return result;
}
void wire_validation() {
    Sim sim(42);
    auto input = frame(sim, 0, 1);
    const auto bytes = serialize_frame(input);
    TickFrame output;
    check(deserialize_frame(bytes, output) && serialize_frame(output) == bytes, "frame roundtrip differs");
    const auto saved = serialize_frame(output);
    for (size_t n = 0; n < bytes.size(); ++n) {
        check(!deserialize_frame(std::span(bytes).first(n), output), "truncated frame accepted");
        check(serialize_frame(output) == saved, "decoder partially changed output");
    }
    for (const size_t offset : {size_t(0), size_t(3), size_t(4), size_t(8), size_t(20), size_t(29), size_t(33)}) {
        auto bad = bytes; bad[offset] = 255;
        check(!deserialize_frame(bad, output), "invalid frame header or length accepted");
    }
    auto bad = bytes; bad.push_back(0);
    check(!deserialize_frame(bad, output), "trailing frame byte accepted");
    bad.assign(kMaxFrameBytes + 1, 0);
    check(!deserialize_frame(bad, output), "oversized packet accepted");
    input.commands[0].units = {2, 1};
    check(serialize_frame(input).empty(), "noncanonical unit order accepted");
    input = frame(sim, 0, 1); input.commands[0].tick++;
    check(serialize_frame(input).empty(), "command tick mismatch accepted");
    input = frame(sim, 0, 1); input.commands[0].player = 1;
    check(serialize_frame(input).empty(), "command player mismatch accepted");
    input = frame(sim, 0, 1); input.commands.push_back(input.commands[0]);
    check(serialize_frame(input).empty(), "duplicate command sequence accepted");
    input = frame(sim, 0, 2); input.commands.push_back(command(0, 0, 1));
    check(serialize_frame(input).empty(), "descending command sequence accepted");
    input = frame(sim, 0); input.tick = std::numeric_limits<uint32_t>::max();
    check(serialize_frame(input).empty(), "wrapping tick accepted");
    input = frame(sim, 0);
    for (uint32_t n = 1; n <= kMaxFrameCommands; ++n) {
        auto c = command(0, 0, n); c.units.clear();
        for (uint32_t id = 1; id <= 256; ++id) c.units.push_back(id);
        input.commands.push_back(c);
    }
    const auto maximum = serialize_frame(input);
    check(maximum.size() == kMaxFrameBytes && deserialize_frame(maximum, output), "maximum bounded frame failed");
    input.commands.push_back(command(0, 0, kMaxFrameCommands + 1));
    check(serialize_frame(input).empty(), "too many commands accepted");
}
void receipt_validation() {
    Lockstep lock;
    auto input = frame(lock.sim(), 0, 1);
    check(lock.receive(input) == ReceiveResult::Accepted, "frame rejected");
    check(lock.receive(input) == ReceiveResult::Duplicate, "duplicate not recognized");
    auto changed = input; changed.previous_hash++;
    check(lock.receive(changed) == ReceiveResult::Conflict, "conflicting duplicate accepted");
    changed = input; changed.commands[0].x++;
    check(lock.receive(changed) == ReceiveResult::Conflict, "changed commands accepted as duplicate");
    auto other = frame(lock.sim(), 1, 1); other.commands[0].units = {1};
    check(lock.receive(other) == ReceiveResult::Invalid, "foreign ownership accepted");
    other.commands[0].units = {999};
    check(lock.receive(other) == ReceiveResult::Invalid, "nonexistent unit accepted");
    other.commands[0].units = {0};
    check(lock.receive(other) == ReceiveResult::Invalid, "zero unit accepted");
    other = frame(lock.sim(), 1); other.player = 2;
    check(lock.receive(other) == ReceiveResult::Invalid, "invalid frame player accepted");
    other = frame(lock.sim(), 1); other.tick = kMaxFutureTicks + 1;
    check(lock.receive(other) == ReceiveResult::TooFar, "future window overflow accepted");
    other.tick = kMaxFutureTicks;
    check(lock.receive(other) == ReceiveResult::Accepted, "future window boundary rejected");
    other = frame(lock.sim(), 1);
    check(lock.receive(other) == ReceiveResult::Accepted && lock.advance() == AdvanceResult::Advanced, "valid pair failed");
    check(lock.receive(input) == ReceiveResult::Stale, "old turn accepted");
}
void stall_and_hashes() {
    Lockstep lock;
    const auto initial = lock.sim().hash(), executed = lock.sim().state_hash();
    check(lock.advance() == AdvanceResult::Waiting, "empty lockstep advances");
    check(lock.receive(frame(lock.sim(), 0, 1)) == ReceiveResult::Accepted, "local frame rejected");
    for (int i = 0; i < 100; ++i) {
        check(lock.advance() == AdvanceResult::Waiting, "missing peer advances");
        check(lock.sim().tick() == 0 && lock.sim().hash() == initial && lock.sim().state_hash() == executed,
              "stall changed authoritative state");
    }
    Sim pending(42), pristine(42);
    check(pending.submit(command(4, 0, 1)), "future command rejected");
    check(pending.state_hash() == pristine.state_hash(), "receive buffer contaminates executed hash");
    check(pending.hash() != pristine.hash(), "legacy hash stopped covering pending input");
    Sim ordered(42), empty(42);
    auto stop = command(0, 0, 1); stop.order = Order::Stop;
    check(ordered.submit(stop), "stop command rejected");
    ordered.step(); empty.step();
    check(ordered.units()[0].x == empty.units()[0].x && ordered.state_hash() != empty.state_hash(),
          "executed sequence not hashed");
}
void reorder_and_determinism() {
    Sim reference(42);
    std::vector<std::array<TickFrame, 2>> turns;
    std::vector<uint64_t> hashes;
    for (uint32_t tick = 0; tick < 12; ++tick) {
        std::array<TickFrame, 2> pair{frame(reference, 0, tick + 1), frame(reference, 1, tick + 1)};
        for (const auto& f : pair) for (const auto& c : f.commands) check(reference.submit(c), "reference submit failed");
        reference.step(); turns.push_back(pair); hashes.push_back(reference.state_hash());
    }
    Lockstep reverse, forward;
    for (const auto& pair : turns) for (const auto& f : pair)
        check(forward.receive(f) == ReceiveResult::Accepted, "ordered future rejected");
    for (size_t tick = turns.size(); tick-- > 0; ) for (int player = 1; player >= 0; --player)
        check(reverse.receive(turns[tick][player]) == ReceiveResult::Accepted, "reordered future rejected");
    for (size_t tick = 0; tick < turns.size(); ++tick) {
        check(forward.advance() == AdvanceResult::Advanced && reverse.advance() == AdvanceResult::Advanced,
              "buffered pair failed to advance");
        check(forward.sim().state_hash() == hashes[tick] && reverse.sim().state_hash() == hashes[tick],
              "packet ordering changed executed result");
    }
}
void rejection_atomicity() {
    Lockstep lock;
    for (uint8_t player = 0; player < 2; ++player)
        check(lock.receive(frame(lock.sim(), player, 1)) == ReceiveResult::Accepted, "initial frame rejected");
    check(lock.advance() == AdvanceResult::Advanced, "initial pair did not advance");
    const auto before = lock.sim().hash();
    check(lock.receive(frame(lock.sim(), 0, 2)) == ReceiveResult::Accepted, "valid first frame rejected");
    check(lock.receive(frame(lock.sim(), 1, 1)) == ReceiveResult::Accepted, "state-relative invalid frame not buffered");
    check(lock.advance() == AdvanceResult::Invalid, "reused executed sequence accepted");
    check(lock.sim().tick() == 1 && lock.sim().hash() == before, "invalid second player partially mutated first player");
    check(lock.advance() == AdvanceResult::Invalid && lock.sim().hash() == before, "invalid retry mutated state");
    Lockstep desync;
    auto wrong = frame(desync.sim(), 1); wrong.previous_hash ^= 1;
    const auto pristine = desync.sim().hash();
    check(desync.receive(frame(desync.sim(), 0, 1)) == ReceiveResult::Accepted &&
          desync.receive(wrong) == ReceiveResult::Accepted, "desync pair receipt failed");
    check(desync.advance() == AdvanceResult::Desync && desync.sim().hash() == pristine,
          "desync advanced or mutated state");
}
}
int main() {
    try {
        wire_validation(); receipt_validation(); stall_and_hashes(); reorder_and_determinism(); rejection_atomicity();
        std::cout << "lockstep tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "lockstep tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
