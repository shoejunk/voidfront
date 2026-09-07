#pragma once
#include "lockstep.hpp"
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace vf::net {
inline constexpr uint32_t kMaxSessionTicks = 100000;
inline constexpr uint32_t kVerificationLag = 16;
inline constexpr unsigned kMaxReceivePacketsPerPoll = 256;
struct SessionOptions {
    uint32_t player=2, port=0, remote_port=0, seed=42, count=6, ticks=2000, timeout=5000;
    uint32_t protocol=vf::kLockstepProtocolVersion, desync=UINT32_MAX, exit_tick=UINT32_MAX;
    uint64_t session=0, content=vf::kLockstepContentId;
    uint32_t input_delay=2;
};
struct SessionStats {
    struct CommandTiming { uint32_t source_tick,execution_tick,sequence; double generated_ms,executed_ms; };
    uint64_t sent=0, received=0, retransmits=0, stalls=0, stall_ms=0;
    uint32_t tick=0;
    uint64_t hash=0;
    uint32_t confirmed_ticks=0, max_verification_lag=0, max_unacked_frames=0, max_unacked_checksums=0;
    uint32_t first_divergent_tick=UINT32_MAX;
    uint64_t advanced_without_ack=0, command_count=0;
    double pacing_elapsed_ms=0;
    std::vector<double> command_latency_ms, tick_interval_ms;
    std::vector<CommandTiming> command_timings;
    std::vector<double> tick_times_ms;
    std::vector<double> tick_deadline_ms,tick_lateness_ms;
    std::optional<double> session_ready_ms;
};

enum class SessionStatus { Handshake, Readiness, Running, Stalled, Finishing, Complete, Error };
struct PollResult {
    SessionStatus status;
    bool advanced;
};
// The provider is sampled once from source state t after initial readiness and
// schedules commands at t+input_delay. It owns sequence generation through the
// supplied counter; Session overwrites command.tick, then validates the canonical
// frame and ownership. Empty output closes an empty turn. No default AI is hidden
// in the adapter. Providers must return promptly, must not reenter Session, and
// must not modify the authoritative state. Human input can be queued externally.
using CommandProvider = std::function<std::vector<Command>(const Sim&, uint8_t, uint32_t&)>;
void validate_session_options(const SessionOptions& options);

// Windows loopback UDP adapter, protocol 2. Single-thread use only. poll() never
// sleeps or blocks waiting for transport: it drains at most 256 datagrams and
// advances at most one simulation tick. Provider/simulation cost and scheduler
// delays are not bounded in wall time; callers must poll frequently for 20 Hz.
// Timer resolution belongs to the caller: this adapter does not request a Windows
// timer period. The CLI retains scoped timeBeginPeriod(1) around its Sleep(1) loop;
// a Godot host must validate its own polling cadence and input latency.
// Construction allocates match-bounded history (<=100000 ticks) and binds a
// socket. All socket/WinSock lifetime ends on completion, error, cancel or
// destruction. Destroying a session does not notify its peer; the peer times out.
// Completion includes the full terminal retry linger. This is not yet Godot
// integration, Internet transport, reconnect, authentication or production netcode.
class Session {
public:
    explicit Session(SessionOptions options, CommandProvider provider);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    PollResult poll();
    void cancel();
    SessionStatus status() const;
    const std::string& error() const;
    const Sim& sim() const;
    const SessionStats& stats() const;
    // Executed frames in player order for this poll only, even if that poll
    // detects a post-advance error. Consume before the next poll to record an
    // applied-only replay prefix. Read-only references are invalid at destruction;
    // callers needing a persistent presentation snapshot must copy sim().units().
    const std::optional<std::array<TickFrame, 2>>& applied_frames() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
