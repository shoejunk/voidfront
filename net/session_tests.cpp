// Direct adapter contract tests. Separate-process/impairment evidence belongs to
// tools/verify_network.py; these paired sessions run in one test process.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include "session.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {
using namespace vf;
using namespace vf::net;
using Clock = std::chrono::steady_clock;

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// Ask Windows for two available ports, keeping both reservations until their
// numbers are known. Session itself must establish its exclusive binding.
struct Ports {
    std::array<SOCKET, 2> sockets{INVALID_SOCKET, INVALID_SOCKET};
    std::array<uint32_t, 2> numbers{};
    Ports() {
        WSADATA data{};
        check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "test WSAStartup");
        try {
            for (size_t i = 0; i < sockets.size(); ++i) {
                sockets[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                check(sockets[i] != INVALID_SOCKET, "test socket allocation");
                BOOL exclusive = TRUE;
                check(setsockopt(sockets[i], SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                    reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == 0, "test exclusive option");
                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                check(bind(sockets[i], reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0,
                    "test ephemeral bind");
                int length = sizeof(address);
                check(getsockname(sockets[i], reinterpret_cast<sockaddr*>(&address), &length) == 0,
                    "test bound address");
                numbers[i] = ntohs(address.sin_port);
            }
        } catch (...) { release(); WSACleanup(); throw; }
    }
    void release() {
        for (auto& socket : sockets) {
            if (socket != INVALID_SOCKET) closesocket(socket);
            socket = INVALID_SOCKET;
        }
    }
    ~Ports() { release(); WSACleanup(); }
    Ports(const Ports&) = delete;
    Ports& operator=(const Ports&) = delete;
};

std::array<SessionOptions, 2> options(Ports& ports, uint32_t ticks = 8) {
    static uint64_t identity = 100;
    std::array<SessionOptions, 2> result{};
    ++identity;
    for (uint32_t player = 0; player < 2; ++player) {
        auto& o = result[player];
        o.player = player; o.port = ports.numbers[player]; o.remote_port = ports.numbers[1 - player];
        o.session = identity; o.ticks = ticks; o.count = 1; o.timeout = 1000;
    }
    ports.release();
    return result;
}

const CommandProvider empty = [](const Sim&, uint8_t, uint32_t&) { return std::vector<Command>{}; };

// Bind the actual configured peer endpoint; packets from another source port
// would only exercise the socket's unrelated-endpoint filter.
struct WirePeer {
    SOCKET handle = INVALID_SOCKET;
    sockaddr_in destination{};
    explicit WirePeer(const SessionOptions& local) {
        handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        check(handle != INVALID_SOCKET, "wire peer socket");
        sockaddr_in source{}; source.sin_family = AF_INET;
        source.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        source.sin_port = htons(static_cast<u_short>(local.remote_port));
        if (bind(handle, reinterpret_cast<const sockaddr*>(&source), sizeof(source)) != 0) {
            closesocket(handle); handle = INVALID_SOCKET;
            throw std::runtime_error("wire peer bind");
        }
        destination = source; destination.sin_port = htons(static_cast<u_short>(local.port));
    }
    ~WirePeer() { if (handle != INVALID_SOCKET) closesocket(handle); }
    void send(const SessionOptions& peer, uint32_t kind, std::vector<uint8_t> body = {}) {
        std::vector<uint8_t> bytes;
        const auto put = [&](uint64_t value, size_t width) {
            for (size_t i = 0; i < width; ++i) bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
        };
        put(0x32504656, 4); put(peer.protocol, 4); put(peer.content, 8); put(peer.session, 8);
        put(peer.player, 4); put(peer.seed, 4); put(peer.count | (peer.map << 16), 4);
        put(peer.ticks, 4); put(kind, 4);
        bytes.insert(bytes.end(), body.begin(), body.end());
        check(sendto(handle, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0,
            reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)) == static_cast<int>(bytes.size()),
            "wire peer send");
    }
};

template<class Done, class Step>
void until(Done done, Step step) {
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (!done()) {
        check(Clock::now() < deadline, "session test watchdog expired");
        step();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// Exercise serialized applied frames through a plain Sim independently of the
// adapter/Lockstep. Keep full hash prefixes for comparison between peers.
struct Recorded {
    Sim replay{42, 1};
    std::vector<uint64_t> hashes;
    uint32_t commands = 0;
    PollResult poll(Session& session) {
        const auto before = session.sim().tick();
        const auto result = session.poll();
        const auto after = session.sim().tick();
        check(after == before || after == before + 1, "poll advanced more than one tick");
        check(result.advanced == (after == before + 1), "advanced flag differs from authoritative progress");
        check(result.status == session.status(), "poll status differs from accessor");
        check(session.stats().tick == after && session.stats().hash == session.sim().state_hash(),
            "stats differ from authoritative snapshot");
        check(session.applied_frames().has_value() == result.advanced, "applied frame lifetime differs from poll");
        if (result.advanced) {
            check(replay.tick() == before, "replay prefix skipped a tick");
            for (uint8_t player = 0; player < 2; ++player) {
                const auto& applied = (*session.applied_frames())[player];
                check(applied.tick == before && applied.player == player, "applied frames not in canonical player order");
                const auto bytes = serialize_frame(applied);
                TickFrame decoded;
                check(!bytes.empty() && deserialize_frame(bytes, decoded), "applied frame wire round trip failed");
                for (const auto& command : decoded.commands) {
                    check(replay.submit(command), "replay rejected applied command");
                    ++commands;
                }
            }
            replay.step();
            check(replay.state_hash() == session.sim().state_hash(), "applied-only replay diverged");
            hashes.push_back(replay.state_hash());
        }
        return result;
    }
};

void healthy(const Session& session) {
    check(session.status() != SessionStatus::Error, "unexpected session failure: " + session.error());
}

void reusable(const SessionOptions& o) {
    Session replacement(o, empty);
    check(replacement.status() == SessionStatus::Handshake, "replacement did not initialize");
    replacement.cancel();
}

void lifetime_and_cancel() {
    Ports ports;
    const auto o = options(ports);
    bool rejected = false;
    try { Session missing(o[0], {}); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "missing provider accepted");
    {
        Session session(o[0], empty);
        rejected = false;
        try { Session duplicate(o[0], empty); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "active exclusive endpoint allowed a second session");
        // No peer: repeated polls must return without advancing or waiting for it.
        for (int i = 0; i < 5; ++i) {
            const auto result = session.poll();
            check(!result.advanced && result.status == SessionStatus::Handshake, "absent peer advanced session");
        }
        const auto initial_hash = session.sim().state_hash();
        session.cancel();
        const auto message = session.error();
        check(session.status() == SessionStatus::Error && message.find("cancelled") != std::string::npos,
            "cancel did not publish terminal error");
        reusable(o[0]); // Original object is still alive.
        session.cancel();
        check(!session.poll().advanced && session.error() == message && session.sim().state_hash() == initial_hash,
            "terminal cancellation changed on subsequent calls");
    }
    { Session destroyed(o[0], empty); }
    reusable(o[0]);
}

Command custom_command(uint8_t player, uint32_t sequence, uint32_t tick, Order order) {
    Command command;
    command.player = player; command.sequence = sequence; command.tick = tick; command.order = order;
    command.units = {uint32_t(player) + 1};
    command.x = (player == 0 ? 8 : 23) * kScale + kScale / 2;
    command.z = 9 * kScale + kScale / 2;
    return command;
}

void successful_pair(bool custom) {
    Ports ports;
    const auto o = options(ports);
    std::array<std::vector<uint32_t>, 2> sampled;
    const CommandProvider provider = [&](const Sim& sim, uint8_t player, uint32_t& sequence) {
        sampled[player].push_back(sim.tick());
        if (custom && (sim.tick() == 0 || sim.tick() == 2)) {
            // Deliberately wrong tick: the adapter must schedule t+input_delay.
            return std::vector<Command>{custom_command(player, ++sequence, UINT32_MAX,
                sim.tick() == 0 ? Order::Move : Order::Hold)};
        }
        return std::vector<Command>{};
    };
    Session first(o[0], provider), second(o[1], provider);
    std::array<Recorded, 2> recording;
    std::array<Sim, 2> expected{Sim(42, 1), Sim(42, 1)};
    const auto tick = [&](Session& session, size_t index) {
        const auto result = recording[index].poll(session);
        healthy(session);
        if (result.advanced) {
            auto& reference = expected[index];
            if (custom && (reference.tick() == 2 || reference.tick() == 4)) {
                for (uint8_t player = 0; player < 2; ++player)
                    check(reference.submit(custom_command(player, reference.tick() == 2 ? 1u : 2u,
                        reference.tick(), reference.tick() == 2 ? Order::Move : Order::Hold)), "expected command rejected");
            }
            reference.step();
            check(reference.state_hash() == session.sim().state_hash(), "injected canonical input differs from independent schedule");
        }
    };
    until([&] { return first.sim().tick() >= 1 && second.sim().tick() >= 1; }, [&] { tick(first, 0); tick(second, 1); });
    // Force several missed pacing deadlines. Each poll must still emit at most
    // one authoritative tick; this is a semantic check, not a speed threshold.
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    tick(first, 0); tick(second, 1);
    until([&] { return first.status() == SessionStatus::Complete && second.status() == SessionStatus::Complete; },
        [&] { tick(first, 0); tick(second, 1); });
    check(recording[0].hashes == recording[1].hashes && recording[0].hashes.size() == o[0].ticks,
        "successful peers have unequal executed prefixes");
    for (size_t player = 0; player < 2; ++player) {
        check(sampled[player] == std::vector<uint32_t>({0, 1, 2, 3, 4, 5}), "provider not sampled exactly once per eligible source tick");
        check(recording[player].commands == (custom ? 4u : 0u), "unexpected canonical command count");
    }
    check(first.stats().confirmed_ticks == o[0].ticks && second.stats().confirmed_ticks == o[1].ticks,
        "completion preceded checksum confirmation");
    reusable(o[0]); reusable(o[1]); // Completion releases ports before destruction.
    const auto final_hash = first.sim().state_hash();
    check(!first.poll().advanced && !first.applied_frames() && first.sim().state_hash() == final_hash,
        "completed poll reapplied terminal frames");
}

void rejected_provider(int mode) {
    Ports ports;
    const auto o = options(ports);
    const CommandProvider invalid = [mode](const Sim&, uint8_t player, uint32_t& sequence) -> std::vector<Command> {
        if (mode == 0) throw std::runtime_error("provider test exception");
        auto command = custom_command(player, ++sequence, 0, Order::Move);
        if (mode == 1) command.units = {2}; // Opponent's unit under local player identity.
        if (mode == 2) command.player = 1; // Noncanonical frame/player identity.
        return {command};
    };
    Session first(o[0], invalid), second(o[1], empty);
    Recorded recording;
    const auto initial_hash = first.sim().state_hash();
    until([&] { return first.status() == SessionStatus::Error; }, [&] { recording.poll(first); second.poll(); });
    check(first.sim().tick() == 0 && first.sim().state_hash() == initial_hash && !first.applied_frames(),
        "invalid provider mutated authoritative state");
    const auto message = first.error();
    check(message.find(mode == 0 ? "provider test exception" : "local scheduled frame rejected") != std::string::npos,
        "provider error diagnostic was lost: " + message);
    reusable(o[0]); // Error releases endpoint while state remains inspectable.
    check(!first.poll().advanced && first.error() == message, "provider terminal state was not stable");
    second.cancel();
}

void post_advance_failure_retains_frames() {
    Ports ports;
    auto o = options(ports);
    o[0].desync = 2;
    Session first(o[0], empty), second(o[1], empty);
    std::array<Recorded, 2> recording;
    until([&] { return first.sim().tick() == 1 && second.sim().tick() == 1; }, [&] {
        if (first.sim().tick() == 0) recording[0].poll(first);
        if (second.sim().tick() == 0) recording[1].poll(second);
        healthy(first); healthy(second);
    });
    // Pause player 1 at tick 1. Player 0 can execute the second initial empty
    // turn and send its deliberately bad state-2 checksum before player 1 does.
    until([&] { return first.sim().tick() == 2; }, [&] { recording[0].poll(first); healthy(first); });
    recording[0].poll(first); // Sends checksum from the preceding advance.
    bool advanced_failure = false;
    until([&] { return second.status() == SessionStatus::Error; }, [&] {
        const auto result = recording[1].poll(second);
        if (result.status == SessionStatus::Error) advanced_failure = result.advanced;
    });
    check(advanced_failure && second.sim().tick() == 2 && second.applied_frames().has_value(),
        "post-advance failure discarded this poll's applied frame prefix");
    check(second.stats().first_divergent_tick == 2 && second.error().find("desync at tick 2") != std::string::npos,
        "post-advance desync diagnostic missing");
    check(recording[0].hashes == recording[1].hashes && recording[1].hashes.size() == 2,
        "failed applied prefix cannot replay identically");
    reusable(o[1]);
    check(!recording[1].poll(second).advanced && !second.applied_frames(), "terminal poll repeated failed applied frames");
    first.cancel();
}

void fresh_identity_reuses_endpoints() {
    Ports ports;
    auto old = options(ports, 4);
    for (auto& side : old) { side.tick_ms = 1; side.timeout = 200; }
    {
        Session first(old[0], empty), second(old[1], empty);
        until([&] { return first.status() == SessionStatus::Complete && second.status() == SessionStatus::Complete; },
            [&] { first.poll(); second.poll(); healthy(first); healthy(second); });
        check(first.stats().confirmed_ticks == 4 && second.stats().confirmed_ticks == 4,
            "original match did not finish before endpoint reuse");
    }
    auto next = old;
    for (auto& side : next) ++side.session;
    uint32_t sampled = 0;
    const CommandProvider tracked = [&](const Sim&, uint8_t, uint32_t&) {
        ++sampled; return std::vector<Command>{};
    };
    Session first(next[0], tracked);
    const auto initial_hash = first.sim().state_hash();
    {
        WirePeer wire(next[0]);
        // Reordered old terminal/ACK/checksum/frame/hello envelopes, including
        // an older protocol/content/setup, must not join or validate payloads.
        auto stale = old[1]; --stale.protocol; ++stale.content; ++stale.seed;
        for (const auto kind : {5u, 4u, 3u, 7u, 6u, 2u, 1u, 2u, 4u}) wire.send(stale, kind);
        for (int i = 0; i < 5; ++i) {
            check(!first.poll().advanced && first.status() == SessionStatus::Handshake,
                "stale traffic changed fresh session readiness");
        }
        check(first.stats().received == 9 && sampled == 0 && first.sim().state_hash() == initial_hash,
            "stale datagrams were not consumed without authoritative effects");
    }
    Session second(next[1], tracked);
    std::array<Recorded, 2> recording;
    until([&] { return first.status() == SessionStatus::Complete && second.status() == SessionStatus::Complete; },
        [&] { recording[0].poll(first); recording[1].poll(second); healthy(first); healthy(second); });
    check(recording[0].hashes == recording[1].hashes && recording[0].hashes.size() == 4 &&
        recording[0].commands == 0 && recording[1].commands == 0 && sampled == 4,
        "fresh match after stale traffic did not replay canonically");
}

void stale_traffic_cannot_keep_session_alive() {
    Ports ports;
    auto o = options(ports);
    o[0].timeout = 200;
    Session first(o[0], empty);
    WirePeer wire(o[0]);
    auto stale = o[1]; --stale.session;
    until([&] { return first.status() == SessionStatus::Error; }, [&] {
        wire.send(stale, 1, {2, 0, 0, 0}); first.poll();
    });
    check(first.sim().tick() == 0 && !first.stats().session_ready_ms &&
        first.error().find("timeout/disconnect waiting for handshake") != std::string::npos,
        "stale traffic suppressed timeout or changed readiness");
}

void current_identity_mismatch_is_terminal() {
    const std::array<std::string, 7> messages{"protocol version", "content identity", "player assignment",
        "match setup", "match setup", "match setup", "input delay"};
    for (size_t mode = 0; mode < messages.size(); ++mode) {
        Ports ports;
        auto o = options(ports);
        Session first(o[0], empty);
        WirePeer wire(o[0]);
        auto peer = o[1];
        if (mode == 0) ++peer.protocol;
        if (mode == 1) ++peer.content;
        if (mode == 2) peer.player = 0;
        if (mode == 3) ++peer.seed;
        if (mode == 4) ++peer.count;
        if (mode == 5) ++peer.ticks;
        wire.send(peer, 1, {static_cast<uint8_t>(mode == 6 ? 3 : 2), 0, 0, 0});
        until([&] { return first.status() == SessionStatus::Error; }, [&] { first.poll(); });
        check(first.sim().tick() == 0 && !first.stats().session_ready_ms &&
            first.error().find(messages[mode]) != std::string::npos,
            "current-identity mismatch lost strict rejection: " + first.error());
    }
}
}

// Economy matches end at the first authoritative winner tick instead of the cap.
void early_outcome_completion() {
    Ports ports;
    auto o = options(ports, 16000);
    for (auto& side : o) { side.map = 2; side.count = 3; side.tick_ms = 1; side.timeout = 1500; }
    Session first(o[0], empty), second(o[1], make_ai_commands);
    const auto deadline = Clock::now() + std::chrono::seconds(60);
    while (first.status() != SessionStatus::Complete || second.status() != SessionStatus::Complete) {
        check(Clock::now() < deadline, "early outcome watchdog: tick " + std::to_string(first.sim().tick()) +
            " winner " + std::to_string(first.sim().winner()));
        first.poll(); second.poll(); healthy(first); healthy(second);
        std::this_thread::yield();
    }
    check(first.sim().winner() == 1 && second.sim().winner() == 1, "early session did not reach the authoritative winner");
    check(first.stats().tick == second.stats().tick && first.stats().tick < 16000, "session ran to the cap after the outcome");
    check(first.stats().hash == second.stats().hash && first.stats().confirmed_ticks == first.stats().tick,
        "early terminal state not confirmed identically");
}

int main() {
    try {
        lifetime_and_cancel();
        successful_pair(false);
        successful_pair(true);
        for (int mode = 0; mode < 3; ++mode) rejected_provider(mode);
        post_advance_failure_retains_frames();
        fresh_identity_reuses_endpoints();
        stale_traffic_cannot_keep_session_alive();
        current_identity_mismatch_is_terminal();
        early_outcome_completion();
        std::cout << "Session API: lifetime/cancel, providers/replay, overdue polling, failed applied prefix, fresh endpoint reuse, stale timeout and current-identity rejection passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Session API failure: " << error.what() << '\n';
        return 1;
    }
}
