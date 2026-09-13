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
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace {
constexpr uint32_t max_ticks = 10000000, max_frames = 100000;
constexpr size_t max_replay_bytes = 64 * 1024 * 1024;
struct Replay {
    uint32_t seed = 42, count = 6, ticks = 2000;
    vf::Map map = vf::Map::Foundry;
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
    if (bytes[0]!='V' || bytes[1]!='F' || bytes[2]!='R' || bytes[3]<1 || bytes[3]>3) throw std::runtime_error("incompatible replay container");
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
    if (bytes[3]>=2) {
        const uint64_t low=read(), high=read();
        if ((low | (high << 32))!=vf::kLockstepContentId) throw std::runtime_error("incompatible replay content");
    }
    if (bytes[3]==3) {
        replay.map=static_cast<vf::Map>(read());
        vf::map_width(replay.map); // Reject an unknown setup before opening outputs.
    }
    if (replay.count<1 || replay.count>250 || replay.ticks<1 || replay.ticks>max_ticks || frame_count>max_frames)
        throw std::runtime_error("replay header outside limits");
    replay.commands.reserve(frame_count);
    for(uint32_t i=0;i<frame_count;++i) {
        const auto size=read();
        if(size<30 || size>26+256*4 || size>bytes.size()-pos) throw std::runtime_error("invalid or truncated replay frame");
        vf::Command c;
        if(!vf::deserialize_command(std::span(bytes).subspan(pos,size),c) || c.tick>=replay.ticks ||
            c.x>=vf::map_width(replay.map)*vf::kScale || c.z>=vf::map_height(replay.map)*vf::kScale)
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
std::vector<vf::Command> profile_commands(const vf::Sim& sim,uint8_t player,uint32_t& sequence,const std::string& profile) {
    if (profile=="ai") return vf::make_ai_commands(sim,player,sequence);
    const auto tick=sim.tick();
    const bool repeat=profile=="repeated" && tick<=800 && tick%80==0;
    const bool pause=profile=="moving-blockers" && player==0 && tick==200;
    const bool resume=profile=="moving-blockers" && player==0 && tick==300;
    if (tick!=0 && !repeat && !pause && !resume) return {};
    vf::Command c{tick,++sequence,player,pause?vf::Order::Stop:vf::Order::Move,{},
        (player==0?108:20)*256+128,64*256+128};
    for (const auto& u:sim.units()) if (u.player==player && u.hp>0 && ((!pause && !resume) || u.id<=16)) c.units.push_back(u.id);
    return c.units.empty()?std::vector<vf::Command>{}:std::vector<vf::Command>{std::move(c)};
}
uint64_t peak_resident_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS info{}; info.cb=sizeof(info);
    if (!GetProcessMemoryInfo(GetCurrentProcess(),&info,sizeof(info))) throw std::runtime_error("memory query failed");
    return info.PeakWorkingSetSize;
#else
    return 0; // Unsupported; not a measured zero and cannot satisfy a memory gate.
#endif
}
}

int main(int argc, char** argv) {
    // Wall-clock measurement is confined to this non-authoritative harness.
    uint32_t ticks = 2000, count = 6, seed = 42;
    std::string trace_path, record_path, replay_path,metrics_path,samples_path,profile="ai";
    vf::Map map=vf::Map::Foundry;
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
            else if (arg == "--metrics") metrics_path = argv[++i];
            else if (arg == "--samples") samples_path = argv[++i];
            else if (arg == "--map") {
                const std::string name=argv[++i]; explicit_setup=true;
                if (name!="foundry" && name!="scale128") throw std::invalid_argument("map must be foundry or scale128");
                map=name=="foundry"?vf::Map::Foundry:vf::Map::Scale128;
            }
            else if (arg == "--profile") {
                profile=argv[++i]; explicit_setup=true;
                if (profile!="ai" && profile!="crossing" && profile!="repeated" && profile!="moving-blockers")
                    throw std::invalid_argument("unsupported benchmark profile");
            }
            else if (arg == "--hash-mode") {
                const std::string mode=argv[++i];
                if (mode!="full" && mode!="state") throw std::invalid_argument("hash-mode must be full or state");
                state_trace=mode=="state";
            }
            else throw std::invalid_argument("unknown option");
        }
        if (!replay_path.empty() && (!record_path.empty() || explicit_setup)) throw std::invalid_argument("replay owns setup; cannot combine with record or setup/profile options");
        if (same_path(trace_path,replay_path) || same_path(trace_path,record_path)) throw std::invalid_argument("trace must differ from replay/record path");
        const std::array<std::string,5> paths{trace_path,record_path,replay_path,metrics_path,samples_path};
        for (size_t a=0;a<paths.size();++a) for (size_t b=a+1;b<paths.size();++b)
            if (same_path(paths[a],paths[b])) throw std::invalid_argument("input and output paths must differ");
        Replay replay;
        if (!replay_path.empty()) { replay=load_replay(replay_path); ticks=replay.ticks; seed=replay.seed; count=replay.count; map=replay.map; }
        if (ticks < 1 || ticks > max_ticks) throw std::invalid_argument("ticks must be 1..10000000");
        if (profile!="ai" && map!=vf::Map::Scale128) throw std::invalid_argument("traffic profiles require scale128 map");
        vf::Sim sim(seed,count,map);
        const auto initial=sim.units();
        std::array<uint32_t,2> sequence{};
        std::ofstream trace;
        if (!trace_path.empty()) { trace.open(trace_path); if(!trace) throw std::runtime_error("trace open failed"); }
        std::ofstream samples;
        if (!samples_path.empty()) { samples.open(samples_path); if(!samples) throw std::runtime_error("samples open failed"); }
        const auto sample=[&]() {
            if (!samples.is_open()) return;
            for (const auto& u:sim.units()) samples<<sim.tick()<<','<<u.id<<','<<u.x<<','<<u.z<<','<<u.hp<<','
                <<static_cast<int>(u.order)<<','<<u.goal_x<<','<<u.goal_z<<'\n';
        };
        sample();
        std::ofstream record;
        uint32_t written_frames=0;
        size_t replay_index=0, written_bytes=24;
        if (!record_path.empty()) {
            record.open(record_path,std::ios::binary); if(!record) throw std::runtime_error("record open failed");
            record.write(map==vf::Map::Foundry?"VFR\1":"VFR\3",4); write_u32(record,vf::kProtocolVersion); write_u32(record,seed);
            write_u32(record,count); write_u32(record,ticks); write_u32(record,0);
            if (map!=vf::Map::Foundry) {
                write_u32(record,static_cast<uint32_t>(vf::kLockstepContentId)); write_u32(record,static_cast<uint32_t>(vf::kLockstepContentId>>32));
                write_u32(record,static_cast<uint32_t>(map)); written_bytes=36;
            }
        }
        std::vector<int64_t> durations;
        std::vector<int64_t> active_durations;
        std::vector<int64_t> step_ns;
        std::vector<int64_t> harness_ns;
        std::vector<int64_t> first_motion(count*2,-1);
        std::vector<uint32_t> idle(count*2),max_idle(count*2),last_motion(count*2);
        std::vector<vf::nav::Point> previous;
        for (const auto& u:sim.units()) previous.push_back({u.x,u.z});
        durations.reserve(ticks); active_durations.reserve(ticks);
        if (!metrics_path.empty()) { step_ns.reserve(ticks); harness_ns.reserve(ticks); }
        for (uint32_t tick = 0; tick < ticks; ++tick) {
            const bool active = sim.winner() == -1;
            const auto begin = std::chrono::steady_clock::now();
            if (!replay_path.empty()) {
                while(replay_index<replay.commands.size() && replay.commands[replay_index].tick==tick)
                    if(!sim.submit(replay.commands[replay_index++])) throw std::runtime_error("replay command rejected at tick " + std::to_string(tick));
            } else {
                for (uint8_t p = 0; p < 2; ++p) for (const auto& c : profile_commands(sim,p,sequence[p],profile)) {
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
            const auto step_begin=std::chrono::steady_clock::now();
            sim.step();
            const auto end=std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count();
            durations.push_back(elapsed);
            if (active) active_durations.push_back(elapsed);
            if (!metrics_path.empty()) {
                step_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end-step_begin).count());
                harness_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count());
                for (const auto& u:sim.units()) {
                    const auto i=u.id-1;
                    if (!(previous[i]==vf::nav::Point{u.x,u.z})) {
                        if (first_motion[i]<0) first_motion[i]=sim.tick();
                        last_motion[i]=sim.tick(); idle[i]=0;
                    } else if (u.hp>0 && (u.x!=u.goal_x || u.z!=u.goal_z)) max_idle[i]=std::max(max_idle[i],++idle[i]);
                    else idle[i]=0;
                    previous[i]={u.x,u.z};
                }
            }
            if(trace) trace << sim.tick() << ' ' << (state_trace ? sim.state_hash() : sim.hash()) << '\n';
            sample();
        }
        if(record.is_open()) {
            record.seekp(20); write_u32(record,written_frames); record.flush();
            if(!record) throw std::runtime_error("record write failed");
        }
        if(trace.is_open()) { trace.flush(); if(!trace) throw std::runtime_error("trace write failed"); }
        if(samples.is_open()) { samples.flush(); if(!samples) throw std::runtime_error("samples write failed"); }
        if (!metrics_path.empty()) {
            const auto peak=peak_resident_bytes();
            std::ofstream metrics(metrics_path);
            if (!metrics) throw std::runtime_error("metrics open failed");
            metrics<<"{\"schema\":1,\"map\":"<<static_cast<uint32_t>(map)<<",\"width\":"<<sim.width()<<",\"height\":"<<sim.height()
                <<",\"seed\":"<<seed<<",\"ticks\":"<<ticks<<",\"units_per_team\":"<<count<<",\"protocol\":"<<vf::kProtocolVersion
                <<",\"content_id\":\""<<vf::kLockstepContentId<<"\",\"replay\":"<<(!replay_path.empty()?"true":"false")
                <<",\"winner\":"<<sim.winner()<<",\"peak_resident_bytes\":"<<peak<<",\"terrain\":[";
            bool comma=false;
            for (const auto& r:vf::map_terrain(map)) { if(comma) metrics<<','; comma=true;
                metrics<<'['<<r.min_x<<','<<r.min_z<<','<<r.max_x<<','<<r.max_z<<']'; }
            metrics<<"],\"units\":["; comma=false;
            for (const auto& u:sim.units()) { const auto i=u.id-1; if(comma) metrics<<','; comma=true;
                metrics<<"{\"id\":"<<u.id<<",\"player\":"<<static_cast<int>(u.player)<<",\"start\":["<<initial[i].x<<','<<initial[i].z
                    <<"],\"end\":["<<u.x<<','<<u.z<<"],\"goal\":["<<u.goal_x<<','<<u.goal_z<<"],\"hp\":"<<u.hp
                    <<",\"order\":"<<static_cast<int>(u.order)<<",\"first_motion_tick\":"<<first_motion[i]<<",\"last_motion_tick\":"<<last_motion[i]
                    <<",\"pending_idle_tail\":"<<idle[i]<<",\"max_pending_idle\":"<<max_idle[i]<<'}'; }
            const auto write_samples=[&](const char* name,const std::vector<int64_t>& values) {
                metrics<<"],\""<<name<<"\":["; for(size_t i=0;i<values.size();++i) { if(i) metrics<<','; metrics<<values[i]; }
            };
            write_samples("step_ns",step_ns); write_samples("harness_ns",harness_ns);
            metrics<<"]}\n"; metrics.flush(); if (!metrics) throw std::runtime_error("metrics write failed");
        }
        std::sort(durations.begin(),durations.end());
        std::sort(active_durations.begin(),active_durations.end());
        std::cout << "ticks=" << sim.tick() << " units=" << count*2 << " hash=" << sim.hash() << " winner=" << sim.winner()
            << " p95_us=" << durations[(durations.size()-1)*95/100] << " p99_us=" << durations[(durations.size()-1)*99/100]
            << " active_ticks=" << active_durations.size()
            << " active_p95_us=" << active_durations[(active_durations.size()-1)*95/100]
            << " active_p99_us=" << active_durations[(active_durations.size()-1)*99/100] << '\n';
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
