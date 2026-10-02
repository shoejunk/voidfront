#include "voidfront_sim.hpp"
#include "session.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <algorithm>
#include <cstdio>
#include <deque>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>

using namespace godot;

class VoidfrontBridge : public RefCounted {
    GDCLASS(VoidfrontBridge, RefCounted)
    vf::Sim simulation{1};
    uint32_t human_sequence = 0;
    uint32_t ai_sequence = 0;
    bool ai_enabled = true;
    uint32_t economy_seed = 1;
    std::vector<vf::Command> economy_commands;
    std::unique_ptr<vf::net::Session> network;
    vf::net::SessionOptions network_options;
    std::string network_failure;
    struct Input {
        vf::Command command;
        int64_t input_usec = -1, accepted_usec = -1, sampled_usec = -1, executed_usec = -1;
        int64_t source_tick = -1, execution_tick = -1;
    };
    std::vector<Input> inputs;
    std::deque<size_t> pending_inputs;
    std::vector<size_t> executed_inputs;
    std::vector<std::pair<uint32_t, uint64_t>> trace;
    std::vector<uint8_t> replay_commands;
    uint32_t replay_count = 0, recorded_ticks = 0, issued_sequence = 0;
    uint32_t last_sampled_source = UINT32_MAX;
    bool evidence_complete = true;
    static constexpr size_t max_queued_inputs = 64, max_recorded_commands = 100000;
    static constexpr size_t max_replay_bytes = 64 * 1024 * 1024;

    static int64_t clock_usec() { return static_cast<int64_t>(Time::get_singleton()->get_ticks_usec()); }
    static String hash_string(uint64_t value) {
        char text[17]; std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
        return String(text);
    }
    const vf::Sim& current_sim() const { return network ? network->sim() : simulation; }
    static void append_u32(std::vector<uint8_t>& bytes, uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<uint8_t>(value >> shift));
    }
    void fail_network(const char* message) {
        network_failure = message;
        if (network) network->cancel();
    }
    void clear_network() {
        economy_commands.clear();
        network.reset(); network_failure.clear(); inputs.clear(); pending_inputs.clear();
        executed_inputs.clear(); trace.clear(); replay_commands.clear();
        replay_count = recorded_ticks = issued_sequence = 0; last_sampled_source = UINT32_MAX;
        network_options = {}; evidence_complete = true;
    }
    static const char* state_name(vf::net::SessionStatus status) {
        using S = vf::net::SessionStatus;
        switch (status) {
        case S::Handshake: return "handshake";
        case S::Readiness: return "readiness";
        case S::Running: return "running";
        case S::Stalled: return "stalled";
        case S::Finishing: return "finishing";
        case S::Complete: return "complete";
        case S::Error: return "error";
        }
        return "error";
    }
    static bool command_from_input(vf::Command& command, const vf::Sim& state, uint8_t player,
            int64_t order, const PackedInt32Array& ids, int64_t x, int64_t z) {
        if (order < 0 || order > 3 || ids.size() == 0 || ids.size() > 256 ||
            x < 0 || z < 0 || x >= state.width() * vf::kScale || z >= state.height() * vf::kScale) return false;
        command.player = player; command.order = static_cast<vf::Order>(order);
        command.x = static_cast<int32_t>(x); command.z = static_cast<int32_t>(z);
        for (int64_t i = 0; i < ids.size(); ++i) {
            const int32_t id = ids[i];
            if (id <= 0 || static_cast<size_t>(id) > state.units().size() ||
                    state.units()[static_cast<size_t>(id) - 1].player != player) return false;
            command.units.push_back(static_cast<uint32_t>(id));
        }
        std::sort(command.units.begin(), command.units.end());
        command.units.erase(std::unique(command.units.begin(), command.units.end()), command.units.end());
        return true;
    }
    std::vector<vf::Command> sample_inputs(const vf::Sim& state, uint8_t player, uint32_t& sequence) {
        last_sampled_source = state.tick();
        std::vector<vf::Command> commands;
        commands.reserve(std::min(pending_inputs.size(), static_cast<size_t>(vf::kMaxFrameCommands)));
        while (!pending_inputs.empty() && commands.size() < vf::kMaxFrameCommands) {
            auto& input = inputs[pending_inputs.front()];
            if (sequence == UINT32_MAX || input.command.sequence != sequence + 1 || input.command.player != player)
                throw std::runtime_error("queued input sequence/ownership mismatch");
            ++sequence;
            input.command.tick = state.tick() + network_options.input_delay;
            input.sampled_usec = clock_usec(); input.source_tick = state.tick();
            input.execution_tick = input.command.tick;
            commands.push_back(input.command); pending_inputs.pop_front();
        }
        return commands;
    }
    static Dictionary input_row(const Input& input, bool terminal = false) {
        Dictionary row;
        row["disposition"] = input.executed_usec >= 0 ? "executed" : terminal ? "unapplied" : input.sampled_usec >= 0 ? "scheduled" : "queued";
        row["sequence"] = input.command.sequence; row["input_usec"] = input.input_usec;
        row["accepted_usec"] = input.accepted_usec; row["sampled_usec"] = input.sampled_usec;
        row["executed_usec"] = input.executed_usec; row["source_tick"] = input.source_tick;
        row["execution_tick"] = input.execution_tick; row["player"] = input.command.player;
        row["order"] = static_cast<int>(input.command.order); row["x"] = input.command.x; row["z"] = input.command.z;
        Array ids; for (const auto id : input.command.units) ids.push_back(id);
        row["units"] = ids; return row;
    }
    void record_applied(int64_t observed_usec) {
        // Capture the applied prefix even when this poll discovers a checksum error.
        evidence_complete = false;
        std::vector<uint8_t> turn;
        uint32_t count = 0;
        for (const auto& frame : *network->applied_frames()) for (const auto& command : frame.commands) {
            const auto bytes = vf::serialize_command(command);
            append_u32(turn, static_cast<uint32_t>(bytes.size()));
            turn.insert(turn.end(), bytes.begin(), bytes.end()); ++count;
        }
        if (replay_count + count > max_recorded_commands || 32 + replay_commands.size() + turn.size() > max_replay_bytes)
            throw std::runtime_error("applied replay evidence limit exceeded; retained prefix is incomplete");
        replay_commands.insert(replay_commands.end(), turn.begin(), turn.end());
        replay_count += count; recorded_ticks = network->sim().tick();
        trace.emplace_back(recorded_ticks, network->sim().state_hash());
        for (const auto& command : (*network->applied_frames())[network_options.player].commands) {
            if (command.sequence == 0 || command.sequence > inputs.size())
                throw std::runtime_error("applied input has no original timestamp");
            auto& input = inputs[command.sequence - 1];
            if (input.executed_usec != -1 || input.command.tick != command.tick)
                throw std::runtime_error("applied input timing mismatch");
            input.executed_usec = observed_usec; executed_inputs.push_back(command.sequence - 1);
        }
        evidence_complete = true;
    }
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("reset", "seed", "ai"), &VoidfrontBridge::reset);
        ClassDB::bind_method(D_METHOD("reset_scale", "seed", "units_per_team", "enemy_ai"), &VoidfrontBridge::reset_scale);
        ClassDB::bind_method(D_METHOD("reset_economy", "seed", "enemy_ai"), &VoidfrontBridge::reset_economy);
        ClassDB::bind_method(D_METHOD("can_build", "x", "z"), &VoidfrontBridge::can_build);
        ClassDB::bind_method(D_METHOD("replay_bytes"), &VoidfrontBridge::replay_bytes);
        ClassDB::bind_method(D_METHOD("advance"), &VoidfrontBridge::advance);
        ClassDB::bind_method(D_METHOD("snapshot"), &VoidfrontBridge::snapshot);
        ClassDB::bind_method(D_METHOD("issue", "order", "ids", "x", "z"), &VoidfrontBridge::issue);
        ClassDB::bind_method(D_METHOD("is_blocked", "x", "z"), &VoidfrontBridge::is_blocked);
        ClassDB::bind_method(D_METHOD("network_start", "player", "local_port", "remote_port", "session_id", "input_delay", "ticks"), &VoidfrontBridge::network_start);
        ClassDB::bind_method(D_METHOD("network_poll", "now_usec"), &VoidfrontBridge::network_poll);
        ClassDB::bind_method(D_METHOD("network_status"), &VoidfrontBridge::network_status);
        ClassDB::bind_method(D_METHOD("network_issue", "order", "ids", "x", "z", "input_usec"), &VoidfrontBridge::network_issue);
        ClassDB::bind_method(D_METHOD("network_cancel"), &VoidfrontBridge::network_cancel);
        ClassDB::bind_method(D_METHOD("network_report"), &VoidfrontBridge::network_report);
    }
public:
    void reset(int64_t seed, bool ai) {
        clear_network();
        simulation = vf::Sim(static_cast<uint32_t>(seed));
        human_sequence = 0; ai_sequence = 0; ai_enabled = ai;
    }
    bool reset_scale(int64_t seed, int64_t units_per_team, bool enemy_ai) {
        if (seed < 0 || seed > UINT32_MAX || units_per_team < 1 || units_per_team > 250 ||
                (network && network->status() != vf::net::SessionStatus::Complete &&
                    network->status() != vf::net::SessionStatus::Error)) return false;
        try {
            // Construct before replacing anything so failed setup preserves the current game.
            vf::Sim replacement(static_cast<uint32_t>(seed), static_cast<uint32_t>(units_per_team), vf::Map::Scale128);
            simulation = std::move(replacement);
            clear_network();
            human_sequence = 0; ai_sequence = 0; ai_enabled = enemy_ai;
            return true;
        } catch (...) { return false; }
    }
    void advance() {
        if (network) return;
        if (ai_enabled && (simulation.map() == vf::Map::Economy || simulation.tick() >= 100) && simulation.tick() % 20 == 0) {
            for (auto &command : vf::make_ai_commands(simulation, 1, ai_sequence)) {
                if (simulation.submit(command) && simulation.map() == vf::Map::Economy) {
                    if (economy_commands.size() < max_recorded_commands) economy_commands.push_back(command);
                    else evidence_complete = false;
                }
            }
        }
        simulation.step();
    }
    bool reset_economy(int64_t seed, bool enemy_ai) {
        if (seed < 0 || seed > UINT32_MAX || (network &&
                network->status() != vf::net::SessionStatus::Complete &&
                network->status() != vf::net::SessionStatus::Error)) return false;
        try {
            vf::Sim replacement(static_cast<uint32_t>(seed), 3, vf::Map::Economy);
            simulation = std::move(replacement);
            clear_network(); economy_seed = static_cast<uint32_t>(seed);
            human_sequence = ai_sequence = 0; ai_enabled = enemy_ai;
            return true;
        } catch (...) { return false; }
    }
    bool can_build(int64_t x, int64_t z) const {
        if (network || x < 0 || z < 0 || x >= simulation.width() * vf::kScale ||
                z >= simulation.height() * vf::kScale) return false;
        return simulation.can_build(0, static_cast<int32_t>(x), static_cast<int32_t>(z));
    }
    PackedByteArray replay_bytes() const {
        PackedByteArray result;
        if (network || simulation.map() != vf::Map::Economy || simulation.tick() == 0 || !evidence_complete) return result;
        std::vector<uint8_t> bytes{'V','F','R',3};
        append_u32(bytes, vf::kProtocolVersion); append_u32(bytes, economy_seed);
        append_u32(bytes, 3); append_u32(bytes, simulation.tick());
        const auto count = std::count_if(economy_commands.begin(), economy_commands.end(),
            [this](const auto& c) { return c.tick < simulation.tick(); });
        append_u32(bytes, static_cast<uint32_t>(count));
        append_u32(bytes, static_cast<uint32_t>(vf::kLockstepContentId));
        append_u32(bytes, static_cast<uint32_t>(vf::kLockstepContentId >> 32));
        append_u32(bytes, static_cast<uint32_t>(vf::Map::Economy));
        auto ordered = economy_commands;
        std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
            return std::tie(a.tick, a.player, a.sequence) < std::tie(b.tick, b.player, b.sequence);
        });
        for (const auto& command : ordered) if (command.tick < simulation.tick()) {
            const auto encoded = vf::serialize_command(command);
            append_u32(bytes, static_cast<uint32_t>(encoded.size()));
            bytes.insert(bytes.end(), encoded.begin(), encoded.end());
        }
        result.resize(static_cast<int64_t>(bytes.size()));
        std::copy(bytes.begin(), bytes.end(), result.ptrw());
        return result;
    }
    bool issue(int64_t order, PackedInt32Array ids, int64_t x, int64_t z) {
        if (network || (simulation.map() == vf::Map::Economy && simulation.winner() != -1)) return false;
        if (order < 0 || order > 10 || ids.size() == 0 || ids.size() > 256 ||
            x < 0 || z < 0 || x >= simulation.width() * vf::kScale || z >= simulation.height() * vf::kScale) return false;
        if (human_sequence == UINT32_MAX) return false;
        vf::Command command{};
        command.tick = simulation.tick(); command.sequence = human_sequence + 1;
        command.player = 0; command.order = static_cast<vf::Order>(order);
        command.x = static_cast<int32_t>(x); command.z = static_cast<int32_t>(z);
        for (int64_t i = 0; i < ids.size(); ++i) {
            if (ids[i] <= 0) return false;
            command.units.push_back(static_cast<uint32_t>(ids[i]));
        }
        std::sort(command.units.begin(), command.units.end());
        command.units.erase(std::unique(command.units.begin(), command.units.end()), command.units.end());
        if (!simulation.submit(command)) return false;
        ++human_sequence;
        if (simulation.map() == vf::Map::Economy) {
            if (economy_commands.size() < max_recorded_commands) economy_commands.push_back(command);
            else evidence_complete = false;
        }
        return true;
    }
    bool is_blocked(int64_t x, int64_t z) const {
        if (x < 0 || z < 0 || x >= current_sim().width() || z >= current_sim().height()) return true;
        return current_sim().blocked(static_cast<int>(x), static_cast<int>(z));
    }
    Dictionary snapshot() const {
        const auto& state = current_sim();
        Dictionary result;
        result["map_id"] = static_cast<uint32_t>(state.map());
        result["width"] = state.width(); result["height"] = state.height();
        result["content_id"] = hash_string(vf::kLockstepContentId);
        result["protocol"] = vf::kProtocolVersion;
        result["tick"] = state.tick(); result["winner"] = state.winner();
        result["enemy_ai"] = !network && ai_enabled;
        result["replay_complete"] = evidence_complete;
        result["hash"] = hash_string(network ? state.state_hash() : state.hash());
        Array units;
        for (const auto &unit : state.units()) {
            Dictionary row;
            row["id"] = unit.id; row["player"] = unit.player;
            row["x"] = unit.x; row["z"] = unit.z; row["hp"] = unit.hp;
            row["moving"] = unit.moving; row["target"] = unit.target_id;
            row["target_structure"] = unit.target_structure_id;
            row["cooldown"] = unit.cooldown; row["order"] = static_cast<int>(unit.order);
            row["kind"] = static_cast<int>(unit.kind); row["cargo"] = unit.cargo; row["cargo_kind"] = unit.cargo_kind;
            row["resource_id"] = unit.resource_id; row["work_ticks"] = unit.work_ticks;
            row["returning"] = unit.returning; row["build_id"] = unit.build_id;
            units.push_back(row);
        }
        result["units"] = units;
        Array vision;
        if (state.map()==vf::Map::Economy) for (uint8_t player=0;player<2;++player) {
            PackedByteArray cells; const auto& source=state.vision(player);
            cells.resize(source.size());
            for (size_t i=0;i<source.size();++i) cells.set(i,source[i]);
            vision.push_back(cells);
        }
        result["vision"] = vision;
        Array resources; resources.push_back(state.salvage(0)); resources.push_back(state.salvage(1));
        result["salvage"] = resources;
        Array flux; flux.push_back(state.flux(0)); flux.push_back(state.flux(1));
        result["flux"] = flux;
        Array research; research.push_back(state.research_ticks(0)); research.push_back(state.research_ticks(1));
        result["research_ticks"] = research;
        Array researched; researched.push_back(state.researched(0)); researched.push_back(state.researched(1));
        result["researched"] = researched;
        result["research_cost"] = vf::kResearchFluxCost; result["research_total_ticks"] = vf::kResearchTicks; result["foundry_cost"] = vf::kFoundryCost;
        result["build_duration"] = vf::kBuildTicks;
        result["strider_cost"] = vf::kStriderCost; result["lancer_cost"] = vf::kLancerCost; result["lancer_flux_cost"] = vf::kLancerFluxCost;
        result["train_ticks"] = vf::kProductionTicks;
        result["production_queue_limit"] = vf::kProductionQueueLimit;
        result["population_cap"] = vf::kPopulationCap;
        Array population_used, population_reserved;
        for (uint8_t player = 0; player < 2; ++player) {
            population_used.push_back(state.population_used(player));
            population_reserved.push_back(state.population_reserved(player));
        }
        result["population_used"] = population_used;
        result["population_reserved"] = population_reserved;
        Array results, sequences;
        for (uint8_t player = 0; player < 2; ++player) {
            results.push_back(static_cast<int>(state.command_result(player)));
            sequences.push_back(state.result_sequence(player));
        }
        result["command_results"] = results; result["result_sequences"] = sequences;
        Array structures;
        for (const auto& structure : state.structures()) {
            Dictionary row;
            row["id"] = structure.id; row["player"] = structure.player;
            row["kind"] = static_cast<int>(structure.kind);
            row["x"] = structure.x; row["z"] = structure.z; row["hp"] = structure.hp;
            row["build_ticks"] = structure.build_ticks;
            row["production_queue"] = structure.production_queue; row["queue_lancers"] = structure.queue_lancers;
            row["production_ticks"] = structure.production_ticks;
            row["spawn_blocked"] = structure.spawn_blocked;
            structures.push_back(row);
        }
        result["structures"] = structures;
        Array deposits;
        for (const auto& deposit : state.deposits()) {
            Dictionary row; row["id"] = deposit.id; row["x"] = deposit.x;
            row["z"] = deposit.z; row["remaining"] = deposit.remaining; row["kind"] = deposit.kind; deposits.push_back(row);
        }
        result["deposits"] = deposits;
        return result;
    }
    bool network_start(int64_t player, int64_t local_port, int64_t remote_port,
            int64_t session_id, int64_t input_delay, int64_t ticks) {
        if (network && network->status() != vf::net::SessionStatus::Complete && network->status() != vf::net::SessionStatus::Error)
            return false;
        try {
            clear_network();
            if (player < 0 || player > 1 || local_port < 1024 || local_port > 65535 ||
                    remote_port < 1024 || remote_port > 65535 || session_id <= 0 ||
                    input_delay < 1 || input_delay > 16 || ticks < 1 || ticks > vf::net::kMaxSessionTicks)
                throw std::invalid_argument("invalid network setup: player 0/1, distinct ports 1024..65535, positive session, delay 1..16, ticks 1..100000");
            network_options = {};
            network_options.player = static_cast<uint32_t>(player);
            network_options.port = static_cast<uint32_t>(local_port);
            network_options.remote_port = static_cast<uint32_t>(remote_port);
            network_options.session = static_cast<uint64_t>(session_id);
            network_options.input_delay = static_cast<uint32_t>(input_delay);
            network_options.ticks = static_cast<uint32_t>(ticks);
            network = std::make_unique<vf::net::Session>(network_options,
                [this](const vf::Sim& state, uint8_t owner, uint32_t& sequence) { return sample_inputs(state, owner, sequence); });
            return true;
        } catch (const std::exception& error) { fail_network(error.what()); }
        catch (...) { fail_network("unknown network startup error"); }
        return false;
    }
    bool network_poll(int64_t now_usec) {
        if (!network || !network_failure.empty()) return false;
        bool advanced = false;
        try {
            executed_inputs.clear();
            if (now_usec < 0 || now_usec > clock_usec()) throw std::invalid_argument("poll timestamp must use Godot Time.get_ticks_usec");
            const auto result = network->poll(); advanced = result.advanced;
            if (advanced) record_applied(clock_usec());
        } catch (const std::exception& error) { fail_network(error.what()); }
        catch (...) { fail_network("unknown network poll error"); }
        return advanced;
    }
    int64_t network_issue(int64_t order, PackedInt32Array ids, int64_t x, int64_t z, int64_t input_usec) {
        try {
            if (!network || !network_failure.empty() || !network->stats().session_ready_ms ||
                    (network->status() != vf::net::SessionStatus::Running && network->status() != vf::net::SessionStatus::Stalled) ||
                    pending_inputs.size() >= max_queued_inputs || inputs.size() >= max_recorded_commands ||
                    issued_sequence == UINT32_MAX) return -1;
            const auto now = clock_usec();
            if (input_usec < 0 || input_usec > now) return -1;
            const auto tick = network->sim().tick();
            const auto next_source = tick + (last_sampled_source == tick ? 1u : 0u);
            if (next_source + network_options.input_delay >= network_options.ticks) return -1;
            const auto remaining_slots = static_cast<uint64_t>(network_options.ticks - network_options.input_delay - next_source) * vf::kMaxFrameCommands;
            if (pending_inputs.size() >= remaining_slots) return -1;
            Input input;
            if (!command_from_input(input.command, network->sim(), static_cast<uint8_t>(network_options.player), order, ids, x, z)) return -1;
            input.command.sequence = issued_sequence + 1;
            input.input_usec = input_usec; input.accepted_usec = now;
            inputs.push_back(std::move(input)); pending_inputs.push_back(inputs.size() - 1);
            return ++issued_sequence;
        } catch (const std::exception& error) { fail_network(error.what()); }
        catch (...) { fail_network("unknown network input error"); }
        return -1;
    }
    void network_cancel() {
        try { if (network) network->cancel(); }
        catch (const std::exception& error) { fail_network(error.what()); }
        catch (...) { fail_network("unknown network cancel error"); }
    }
    Dictionary make_network_status() const {
        Dictionary result;
        result["state"] = !network_failure.empty() ? "error" : network ? state_name(network->status()) : "offline";
        result["error"] = String((!network_failure.empty() ? network_failure : network ? network->error() : "").c_str());
        result["player"] = network_options.player; result["input_delay"] = network_options.input_delay;
        result["tick"] = current_sim().tick(); result["hash"] = hash_string(current_sim().state_hash());
        result["ready"] = network && network->stats().session_ready_ms.has_value();
        result["confirmed_ticks"] = network ? network->stats().confirmed_ticks : 0;
        result["queued_inputs"] = static_cast<int64_t>(pending_inputs.size());
        Array events; for (const auto index : executed_inputs) events.push_back(input_row(inputs[index]));
        result["executed_inputs"] = events;
        return result;
    }
    Dictionary make_network_report() const {
        Dictionary result = make_network_status();
        result["seed"] = network_options.seed; result["units_per_team"] = network_options.count;
        result["requested_ticks"] = network_options.ticks; result["recorded_ticks"] = recorded_ticks;
        result["evidence_complete"] = network && evidence_complete && recorded_ticks == network->sim().tick() && trace.size() == recorded_ticks;
        result["timing_clock"] = "Godot Time.get_ticks_usec; executed_usec is first bridge observation after Session.poll returns";
        result["replay_command_count"] = replay_count;
        const bool terminal = !network_failure.empty() || (network &&
            (network->status() == vf::net::SessionStatus::Error || network->status() == vf::net::SessionStatus::Complete));
        Array input_events; for (const auto& input : inputs) input_events.push_back(input_row(input, terminal));
        result["inputs"] = input_events;
        Array rows;
        for (const auto& [tick, hash] : trace) { Dictionary row; row["tick"] = tick; row["hash"] = hash_string(hash); rows.push_back(row); }
        result["trace"] = rows;
        std::vector<uint8_t> bytes{'V', 'F', 'R', 2};
        append_u32(bytes, vf::kProtocolVersion); append_u32(bytes, network_options.seed);
        append_u32(bytes, network_options.count); append_u32(bytes, recorded_ticks); append_u32(bytes, replay_count);
        append_u32(bytes, static_cast<uint32_t>(vf::kLockstepContentId));
        append_u32(bytes, static_cast<uint32_t>(vf::kLockstepContentId >> 32));
        bytes.insert(bytes.end(), replay_commands.begin(), replay_commands.end());
        PackedByteArray replay; replay.resize(static_cast<int64_t>(bytes.size()));
        std::copy(bytes.begin(), bytes.end(), replay.ptrw()); result["replay"] = replay;
        if (network) {
            const auto& stats = network->stats();
            result["sent"] = static_cast<int64_t>(stats.sent); result["received"] = static_cast<int64_t>(stats.received);
            result["retransmits"] = static_cast<int64_t>(stats.retransmits); result["stalls"] = static_cast<int64_t>(stats.stalls);
            result["stall_ms"] = static_cast<int64_t>(stats.stall_ms); result["pacing_elapsed_ms"] = stats.pacing_elapsed_ms;
            result["setup_ms"] = stats.session_ready_ms.value_or(-1);
            Array intervals; for (const auto interval : stats.tick_interval_ms) intervals.push_back(interval);
            result["tick_interval_ms"] = intervals;
        }
        return result;
    }
    Dictionary network_status() {
        try { return make_network_status(); }
        catch (const std::exception& error) { fail_network(error.what()); }
        catch (...) { fail_network("unknown network status error"); }
        Dictionary result; result["state"] = "error"; result["error"] = String(network_failure.c_str()); return result;
    }
    Dictionary network_report() {
        try { return make_network_report(); }
        catch (const std::exception& error) { fail_network(error.what()); }
        catch (...) { fail_network("unknown network report error"); }
        Dictionary result = network_status(); result["evidence_complete"] = false; return result;
    }
};

static void initialize(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) GDREGISTER_CLASS(VoidfrontBridge);
}
static void uninitialize(ModuleInitializationLevel) {}
extern "C" GDExtensionBool GDE_EXPORT voidfront_library_init(GDExtensionInterfaceGetProcAddress address,
        GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization) {
    GDExtensionBinding::InitObject init(address, library, initialization);
    init.register_initializer(initialize); init.register_terminator(uninitialize);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
