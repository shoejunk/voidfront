#include "voidfront_sim.hpp"
#include "lockstep.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

namespace {
constexpr uint32_t max_ticks = 10000000, max_frames = 100000;
constexpr size_t max_replay_bytes = 64 * 1024 * 1024;
struct Replay {
    uint32_t seed = 42, count = 6, ticks = 2000;
    std::vector<vf::Command> commands;
};
void write_u32(std::ostream& stream, uint32_t value) {
    std::array<char,4> bytes{};
    for (int i=0; i<4; ++i) bytes[i] = static_cast<char>((value >> (8*i)) & 255);
    stream.write(bytes.data(), bytes.size());
}
uint32_t number(const char* text) {
    const std::string s(text);
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) throw std::invalid_argument("expected unsigned integer");
    const auto value = std::stoull(s);
    if (value > std::numeric_limits<uint32_t>::max()) throw std::invalid_argument("integer exceeds u32");
    return static_cast<uint32_t>(value);
}
Replay load_replay(const std::string& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("replay open failed");
    const auto length = stream.tellg();
    if (length < 24 || length > static_cast<std::streamoff>(max_replay_bytes)) throw std::runtime_error("replay length outside limits");
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    stream.seekg(0); stream.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!stream) throw std::runtime_error("replay read failed");
    if (bytes[0]!='V' || bytes[1]!='F' || bytes[2]!='R' || (bytes[3]!=1 && bytes[3]!=2)) throw std::runtime_error("incompatible replay container");
    size_t pos=4;
    const auto read = [&]() {
        if (bytes.size()-pos < 4) throw std::runtime_error("truncated replay integer");
        uint32_t value=0;
        for(int i=0;i<4;++i) value |= uint32_t(bytes[pos++]) << (8*i);
        return value;
    };
    if (read()!=vf::kProtocolVersion) throw std::runtime_error("incompatible replay protocol");
    Replay replay;
    replay.seed=read(); replay.count=read(); replay.ticks=read();
    const auto frame_count=read();
    if (bytes[3]==2) {
        const uint64_t low=read(), high=read();
        if ((low | (high << 32))!=vf::kLockstepContentId) throw std::runtime_error("incompatible replay content");
    }
    if (replay.count<1 || replay.count>250 || replay.ticks<1 || replay.ticks>max_ticks || frame_count>max_frames)
        throw std::runtime_error("replay header outside limits");
    replay.commands.reserve(frame_count);
    for(uint32_t i=0;i<frame_count;++i) {
        const auto size=read();
        if(size<30 || size>26+256*4 || size>bytes.size()-pos) throw std::runtime_error("invalid or truncated replay frame");
        vf::Command c;
        if(!vf::deserialize_command(std::span(bytes).subspan(pos,size),c) || c.tick>=replay.ticks)
            throw std::runtime_error("invalid canonical replay command");
        pos+=size;
        if(!replay.commands.empty()) {
            const auto& previous=replay.commands.back();
            if(std::tie(c.tick,c.player,c.sequence)<=std::tie(previous.tick,previous.player,previous.sequence))
                throw std::runtime_error("replay commands out of canonical order");
        }
        replay.commands.push_back(std::move(c));
    }
    if(pos!=bytes.size()) throw std::runtime_error("trailing replay data");
    return replay;
}
bool same_path(const std::string& a,const std::string& b) {
    if (a.empty() || b.empty()) return false;
    if (std::filesystem::exists(a) && std::filesystem::exists(b) && std::filesystem::equivalent(a,b)) return true;
    auto left=std::filesystem::weakly_canonical(a).wstring();
    auto right=std::filesystem::weakly_canonical(b).wstring();
#ifdef _WIN32
    // Also protect two not-yet-created outputs on Windows' default filesystem.
    std::transform(left.begin(),left.end(),left.begin(),std::towlower);
    std::transform(right.begin(),right.end(),right.begin(),std::towlower);
#endif
    return left==right;
}
}

int main(int argc, char** argv) {
    // Wall-clock measurement is confined to this non-authoritative harness.
    uint32_t ticks = 2000, count = 6, seed = 42;
    std::string trace_path, record_path, replay_path;
    bool explicit_setup=false;
    bool state_trace=false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (i + 1 >= argc) throw std::invalid_argument("missing option value");
            if (arg == "--ticks") { ticks = number(argv[++i]); explicit_setup=true; }
            else if (arg == "--units-per-team") { count = number(argv[++i]); explicit_setup=true; }
            else if (arg == "--seed") { seed = number(argv[++i]); explicit_setup=true; }
            else if (arg == "--trace") trace_path = argv[++i];
            else if (arg == "--record") record_path = argv[++i];
            else if (arg == "--replay") replay_path = argv[++i];
            else if (arg == "--hash-mode") {
                const std::string mode=argv[++i];
                if (mode!="full" && mode!="state") throw std::invalid_argument("hash-mode must be full or state");
                state_trace=mode=="state";
            }
            else throw std::invalid_argument("unknown option");
        }
        if (!replay_path.empty() && (!record_path.empty() || explicit_setup)) throw std::invalid_argument("replay owns setup; cannot combine with record, ticks, seed or units-per-team");
        if (same_path(trace_path,replay_path) || same_path(trace_path,record_path)) throw std::invalid_argument("trace must differ from replay/record path");
        Replay replay;
        if (!replay_path.empty()) { replay=load_replay(replay_path); ticks=replay.ticks; seed=replay.seed; count=replay.count; }
        if (ticks < 1 || ticks > max_ticks) throw std::invalid_argument("ticks must be 1..10000000");
        vf::Sim sim(seed,count);
        std::array<uint32_t,2> sequence{};
        std::ofstream trace;
        if (!trace_path.empty()) { trace.open(trace_path); if(!trace) throw std::runtime_error("trace open failed"); }
        std::ofstream record;
        uint32_t written_frames=0;
        size_t replay_index=0, written_bytes=24;
        if (!record_path.empty()) {
            record.open(record_path,std::ios::binary); if(!record) throw std::runtime_error("record open failed");
            record.write("VFR\1",4); write_u32(record,vf::kProtocolVersion); write_u32(record,seed);
            write_u32(record,count); write_u32(record,ticks); write_u32(record,0);
        }
        std::vector<int64_t> durations;
        std::vector<int64_t> active_durations;
        for (uint32_t tick = 0; tick < ticks; ++tick) {
            const bool active = sim.winner() == -1;
            const auto begin = std::chrono::steady_clock::now();
            if (!replay_path.empty()) {
                while(replay_index<replay.commands.size() && replay.commands[replay_index].tick==tick)
                    if(!sim.submit(replay.commands[replay_index++])) throw std::runtime_error("replay command rejected at tick " + std::to_string(tick));
            } else {
                for (uint8_t p = 0; p < 2; ++p) for (const auto& c : vf::make_ai_commands(sim,p,sequence[p])) {
                    if(!sim.submit(c)) throw std::runtime_error("AI command rejected");
                    if(record.is_open()) {
                        const auto bytes=vf::serialize_command(c);
                        if(written_frames==max_frames || written_bytes+4+bytes.size()>max_replay_bytes) throw std::runtime_error("record exceeds replay limits");
                        write_u32(record,static_cast<uint32_t>(bytes.size()));
                        record.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
                        ++written_frames; written_bytes+=4+bytes.size();
                    }
                }
            }
            sim.step();
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();
            durations.push_back(elapsed);
            if (active) active_durations.push_back(elapsed);
            if(trace) trace << sim.tick() << ' ' << (state_trace ? sim.state_hash() : sim.hash()) << '\n';
        }
        if(record.is_open()) {
            record.seekp(20); write_u32(record,written_frames); record.flush();
            if(!record) throw std::runtime_error("record write failed");
        }
        if(trace.is_open()) { trace.flush(); if(!trace) throw std::runtime_error("trace write failed"); }
        std::sort(durations.begin(),durations.end());
        std::sort(active_durations.begin(),active_durations.end());
        std::cout << "ticks=" << sim.tick() << " units=" << count*2 << " hash=" << sim.hash() << " winner=" << sim.winner()
            << " p95_us=" << durations[(durations.size()-1)*95/100] << " p99_us=" << durations[(durations.size()-1)*99/100]
            << " active_ticks=" << active_durations.size()
            << " active_p95_us=" << active_durations[(active_durations.size()-1)*95/100]
            << " active_p99_us=" << active_durations[(active_durations.size()-1)*99/100] << '\n';
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
