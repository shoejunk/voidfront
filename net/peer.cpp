// Loopback-only transport evidence harness; not a production Internet protocol.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mmsystem.h>
#include "lockstep.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
constexpr uint32_t kMagic = 0x32504656, kMaxTicks = 100000;
// The u32 packet type is at byte offset 40 in every datagram.
enum class Kind : uint32_t { Hello=1, Frame=2, Ack=3, Finish=4, FinishAck=5, Checksum=6, ChecksumAck=7 };
constexpr size_t kHeaderBytes=44, kMaxDatagram=60000;
constexpr auto kRetry = std::chrono::milliseconds(40);
constexpr auto kTickPeriod = std::chrono::milliseconds(50);
constexpr uint32_t kVerificationLag=16;
struct Options {
    uint32_t player=2, port=0, remote_port=0, seed=42, count=6, ticks=2000, timeout=5000;
    uint32_t protocol=vf::kLockstepProtocolVersion, desync=UINT32_MAX, exit_tick=UINT32_MAX;
    uint64_t session=0, content=vf::kLockstepContentId;
    uint32_t input_delay=2;
    std::string trace, record, report;
};
struct Stats {
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
};
uint64_t number(const char* raw) {
    const std::string s(raw);
    if(s.empty() || s.find_first_not_of("0123456789")!=std::string::npos)
        throw std::invalid_argument("expected unsigned decimal integer");
    return std::stoull(s);
}
uint32_t u32(const char* raw) {
    const auto n=number(raw);
    if(n>UINT32_MAX) throw std::invalid_argument("integer exceeds u32");
    return static_cast<uint32_t>(n);
}
bool aliases(const std::string& a,const std::string& b) {
    if(a.empty() || b.empty()) return false;
    if(std::filesystem::exists(a) && std::filesystem::exists(b) && std::filesystem::equivalent(a,b)) return true;
    auto left=std::filesystem::weakly_canonical(a).wstring();
    auto right=std::filesystem::weakly_canonical(b).wstring();
    std::transform(left.begin(),left.end(),left.begin(),std::towlower);
    std::transform(right.begin(),right.end(),right.begin(),std::towlower);
    return left==right;
}
Options parse(int argc,char** argv) {
    Options o;
    std::vector<std::string> seen;
    for(int i=1;i<argc;++i) {
        const std::string key=argv[i];
        if(i+1>=argc) throw std::invalid_argument("missing value for "+key);
        if(std::find(seen.begin(),seen.end(),key)!=seen.end()) throw std::invalid_argument("duplicate option "+key);
        seen.push_back(key); const char* val=argv[++i];
        if(key=="--player") o.player=u32(val);
        else if(key=="--port") o.port=u32(val);
        else if(key=="--remote-port") o.remote_port=u32(val);
        else if(key=="--session") o.session=number(val);
        else if(key=="--ticks") o.ticks=u32(val);
        else if(key=="--seed") o.seed=u32(val);
        else if(key=="--units-per-team") o.count=u32(val);
        else if(key=="--timeout-ms") o.timeout=u32(val);
        else if(key=="--input-delay-ticks") o.input_delay=u32(val);
        else if(key=="--content-id") o.content=number(val);
        else if(key=="--protocol-version") o.protocol=u32(val);
        else if(key=="--desync-tick") o.desync=u32(val);
        else if(key=="--exit-at-tick") o.exit_tick=u32(val);
        else if(key=="--trace") o.trace=val;
        else if(key=="--record") o.record=val;
        else if(key=="--report") o.report=val;
        else throw std::invalid_argument("unknown option "+key);
    }
    if(o.player>1 || o.port<1024 || o.port>65535 || o.remote_port<1024 || o.remote_port>65535 || o.port==o.remote_port)
        throw std::invalid_argument("player must be 0/1; distinct ports must be 1024..65535");
    if(o.session==0 || o.ticks<1 || o.ticks>kMaxTicks || o.count<1 || o.count>250 || o.timeout<200 || o.timeout>60000)
        throw std::invalid_argument("session must be nonzero; ticks 1..100000, count 1..250, timeout-ms 200..60000");
    if((o.desync!=UINT32_MAX && (o.desync==0 || o.desync>o.ticks)) || (o.exit_tick!=UINT32_MAX && o.exit_tick>o.ticks))
        throw std::invalid_argument("fault injection tick outside match");
    if(o.input_delay<1 || o.input_delay>16) throw std::invalid_argument("input-delay-ticks must be 1..16");
    if(aliases(o.trace,o.record) || aliases(o.trace,o.report) || aliases(o.record,o.report))
        throw std::invalid_argument("trace, record and report paths must differ");
    return o;
}
void put(std::vector<uint8_t>& b,uint64_t n,size_t width) {
    for(size_t i=0;i<width;++i) b.push_back(static_cast<uint8_t>(n>>(8*i)));
}
uint64_t get(std::span<const uint8_t> b,size_t offset,size_t width) {
    if(offset>b.size() || width>b.size()-offset) throw std::runtime_error("truncated transport packet");
    uint64_t n=0; for(size_t i=0;i<width;++i) n|=uint64_t(b[offset+i])<<(8*i); return n;
}
uint64_t digest(std::span<const uint8_t> b) {
    uint64_t hash=14695981039346656037ULL;
    for(const auto byte:b) { hash^=byte; hash*=1099511628211ULL; } return hash;
}
std::vector<uint8_t> packet(const Options& o,Kind kind,std::span<const uint8_t> body={}) {
    std::vector<uint8_t> b; b.reserve(kHeaderBytes+body.size());
    put(b,kMagic,4); put(b,o.protocol,4); put(b,o.content,8); put(b,o.session,8);
    put(b,o.player,4); put(b,o.seed,4); put(b,o.count,4); put(b,o.ticks,4); put(b,static_cast<uint32_t>(kind),4);
    b.insert(b.end(),body.begin(),body.end());
    if(b.size()>kMaxDatagram) throw std::runtime_error("frame exceeds bounded datagram size");
    return b;
}
Kind header(const Options& o,std::span<const uint8_t> b) {
    if(b.size()<kHeaderBytes || get(b,0,4)!=kMagic) throw std::runtime_error("invalid transport header");
    if(get(b,4,4)!=o.protocol) throw std::runtime_error("incompatible protocol version");
    if(get(b,8,8)!=o.content) throw std::runtime_error("incompatible content identity");
    if(get(b,16,8)!=o.session) throw std::runtime_error("incompatible session");
    if(get(b,24,4)!=1-o.player) throw std::runtime_error("incompatible player assignment");
    if(get(b,28,4)!=o.seed || get(b,32,4)!=o.count || get(b,36,4)!=o.ticks)
        throw std::runtime_error("incompatible match setup (seed/count/ticks)");
    const auto kind=get(b,40,4);
    if(kind<1 || kind>7) throw std::runtime_error("unknown packet kind");
    return static_cast<Kind>(kind);
}
class Socket {
    SOCKET handle_=INVALID_SOCKET;
    sockaddr_in remote_{};
public:
    explicit Socket(const Options& o) {
        WSADATA data{};
        if(WSAStartup(MAKEWORD(2,2),&data)!=0) throw std::runtime_error("WSAStartup failed");
        handle_=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(handle_==INVALID_SOCKET) { WSACleanup(); throw std::runtime_error("UDP socket failed"); }
        sockaddr_in local{}; local.sin_family=AF_INET; local.sin_addr.s_addr=htonl(INADDR_LOOPBACK); local.sin_port=htons(static_cast<u_short>(o.port));
        BOOL exclusive=TRUE;
        if(setsockopt(handle_,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive))!=0 ||
           bind(handle_,reinterpret_cast<const sockaddr*>(&local),sizeof(local))!=0) {
            closesocket(handle_); WSACleanup(); throw std::runtime_error("exclusive loopback bind failed");
        }
        u_long nonblocking=1;
        if(ioctlsocket(handle_,FIONBIO,&nonblocking)!=0) { closesocket(handle_); WSACleanup(); throw std::runtime_error("nonblocking mode failed"); }
        remote_=local; remote_.sin_port=htons(static_cast<u_short>(o.remote_port));
    }
    ~Socket() { if(handle_!=INVALID_SOCKET) closesocket(handle_); WSACleanup(); }
    bool send(std::span<const uint8_t> b) {
        const int n=sendto(handle_,reinterpret_cast<const char*>(b.data()),static_cast<int>(b.size()),0,reinterpret_cast<const sockaddr*>(&remote_),sizeof(remote_));
        if(n==SOCKET_ERROR && WSAGetLastError()==WSAEWOULDBLOCK) return false;
        if(n!=static_cast<int>(b.size())) throw std::runtime_error("UDP send failed "+std::to_string(WSAGetLastError()));
        return true;
    }
    std::optional<std::vector<uint8_t>> receive() {
        std::array<uint8_t,kMaxDatagram> b{}; sockaddr_in source{}; int size=sizeof(source);
        const int n=recvfrom(handle_,reinterpret_cast<char*>(b.data()),static_cast<int>(b.size()),0,reinterpret_cast<sockaddr*>(&source),&size);
        if(n==SOCKET_ERROR) {
            const auto error=WSAGetLastError();
            if(error==WSAEWOULDBLOCK || error==WSAECONNRESET) return std::nullopt;
            throw std::runtime_error("UDP receive failed "+std::to_string(error));
        }
        if(source.sin_family!=AF_INET || source.sin_addr.s_addr!=remote_.sin_addr.s_addr || source.sin_port!=remote_.sin_port)
            return std::vector<uint8_t>{}; // Ignore traffic outside the configured endpoint.
        return std::vector<uint8_t>(b.begin(),b.begin()+n);
    }
};
// Windows' default coarse sleep quantum demonstrably produced ~16 Hz here.
// Scope this presentation/transport scheduling request to this peer process run.
class TimerResolution {
public:
    TimerResolution() {
        if(timeBeginPeriod(1)!=TIMERR_NOERROR) throw std::runtime_error("1ms timer resolution unavailable");
    }
    ~TimerResolution() { timeEndPeriod(1); }
    TimerResolution(const TimerResolution&)=delete;
    TimerResolution& operator=(const TimerResolution&)=delete;
};
void stream_u32(std::ostream& s,uint32_t n) {
    std::vector<uint8_t> bytes; put(bytes,n,4); s.write(reinterpret_cast<const char*>(bytes.data()),4);
}
void close_checked(std::ofstream& stream,const char* name) {
    if(!stream.is_open()) return;
    stream.flush(); if(!stream) throw std::runtime_error(std::string(name)+" flush failed");
    stream.close(); if(stream.fail()) throw std::runtime_error(std::string(name)+" close failed");
}
std::string json_string(const std::string& s) {
    std::string out="\"";
    for(unsigned char c:s) { if(c=='"' || c=='\\') out+='\\'; if(c>=32) out+=static_cast<char>(c); else out+=' '; }
    return out+'"';
}
double milliseconds(Clock::duration duration) { return std::chrono::duration<double,std::milli>(duration).count(); }
double percentile(std::vector<double> values,size_t numerator) {
    if(values.empty()) return 0;
    std::sort(values.begin(),values.end());
    return values[(values.size()*numerator+99)/100-1];
}
void samples(std::ostream& out,const std::vector<double>& values) {
    out<<'[';
    for(size_t i=0;i<values.size();++i) { if(i) out<<','; out<<values[i]; }
    out<<']';
}
void report(const Options& o,const Stats& s,const std::string& status,const std::string& error,Clock::time_point start) {
    if(o.report.empty()) return;
    std::ofstream out(o.report); if(!out) throw std::runtime_error("report open failed");
    out<<std::setprecision(12);
    out<<"{\"status\":"<<json_string(status)<<",\"error\":"<<json_string(error)<<",\"player\":"<<o.player
       <<",\"ticks\":"<<s.tick<<",\"content_id\":\""<<o.content<<"\",\"final_hash\":\""<<s.hash<<"\",\"sent\":"<<s.sent<<",\"received\":"<<s.received
       <<",\"retransmits\":"<<s.retransmits<<",\"stall_count\":"<<s.stalls<<",\"stall_ms\":"<<s.stall_ms
       <<",\"elapsed_ms\":"<<std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count()
       <<",\"input_delay_ticks\":"<<o.input_delay<<",\"verification_lag_limit\":"<<kVerificationLag
       <<",\"timer_period_ms\":1"
       <<",\"max_verification_lag_ticks\":"<<s.max_verification_lag<<",\"confirmed_ticks\":"<<s.confirmed_ticks
       <<",\"max_unacked_frames\":"<<s.max_unacked_frames<<",\"max_unacked_checksums\":"<<s.max_unacked_checksums
       <<",\"advanced_without_ack\":"<<s.advanced_without_ack<<",\"command_count\":"<<s.command_count
       <<",\"pacing_elapsed_ms\":"<<s.pacing_elapsed_ms
       <<",\"pacing_hz\":"<<(s.tick>1 && s.pacing_elapsed_ms>0 ? (s.tick-1)*1000.0/s.pacing_elapsed_ms:0)
       <<",\"command_latency_p95_ms\":"<<percentile(s.command_latency_ms,95)
       <<",\"command_latency_p99_ms\":"<<percentile(s.command_latency_ms,99)
       <<",\"command_latency_max_ms\":"<<percentile(s.command_latency_ms,100)
       <<",\"tick_interval_p95_ms\":"<<percentile(s.tick_interval_ms,95)
       <<",\"tick_interval_p99_ms\":"<<percentile(s.tick_interval_ms,99)
       <<",\"tick_interval_max_ms\":"<<percentile(s.tick_interval_ms,100)
       <<",\"first_divergent_tick\":";
    if(s.first_divergent_tick==UINT32_MAX) out<<"null"; else out<<s.first_divergent_tick;
    out<<",\"command_latency_samples_ms\":"; samples(out,s.command_latency_ms);
    out<<",\"tick_interval_samples_ms\":"; samples(out,s.tick_interval_ms);
    out<<",\"tick_times_ms\":"; samples(out,s.tick_times_ms);
    out<<",\"tick_deadline_samples_ms\":"; samples(out,s.tick_deadline_ms);
    out<<",\"tick_lateness_samples_ms\":"; samples(out,s.tick_lateness_ms);
    out<<",\"command_timings\":[";
    for(size_t i=0;i<s.command_timings.size();++i) {
        const auto& t=s.command_timings[i]; if(i) out<<',';
        out<<"{\"source_tick\":"<<t.source_tick<<",\"execution_tick\":"<<t.execution_tick<<",\"sequence\":"<<t.sequence
           <<",\"generated_ms\":"<<t.generated_ms<<",\"executed_ms\":"<<t.executed_ms
           <<",\"latency_ms\":"<<t.executed_ms-t.generated_ms<<'}';
    }
    out<<']';
    out<<"}\n";
    close_checked(out,"report");
}
void run(const Options& o,Stats& stats) {
    const auto origin=Clock::now();
    TimerResolution timer_resolution;
    Socket socket(o); vf::Lockstep lock(o.seed,o.count);
    stats.hash=lock.sim().state_hash();
    std::ofstream trace,record;
    if(!o.trace.empty()) { trace.open(o.trace); if(!trace) throw std::runtime_error("trace open failed"); }
    if(!o.record.empty()) {
        record.open(o.record,std::ios::binary); if(!record) throw std::runtime_error("record open failed");
        record.write("VFR\2",4); stream_u32(record,vf::kProtocolVersion); stream_u32(record,o.seed);
        stream_u32(record,o.count); stream_u32(record,o.ticks); stream_u32(record,0);
        stream_u32(record,static_cast<uint32_t>(vf::kLockstepContentId)); stream_u32(record,static_cast<uint32_t>(vf::kLockstepContentId>>32));
    }
    struct LocalInput {
        std::optional<vf::TickFrame> frame;
        std::vector<uint8_t> wire;
        uint64_t digest=0;
        bool generated=false,acked=false,receipt_acked=false,sent=false;
        uint32_t source_tick=0;
        Clock::time_point created{},last_send{};
    };
    struct LocalChecksum {
        std::optional<uint64_t> hash;
        uint64_t wire_hash=0;
        bool acked=false,sent=false;
        Clock::time_point last_send{};
    };
    std::vector<LocalInput> local(o.ticks);
    std::vector<std::optional<vf::TickFrame>> remote(o.ticks);
    std::vector<std::optional<uint64_t>> remote_digests(o.ticks),peer_hashes(o.ticks+1);
    std::vector<LocalChecksum> checksums(o.ticks+1);
    uint32_t recorded=0,sequence=0,generated_source=UINT32_MAX;
    size_t record_bytes=32;
    bool joined=false,initialized=false,peer_ready=false,peer_finished=false,finish_acked=false;
    bool hello_sent=false,finish_sent=false;
    std::optional<uint64_t> pending_finish;
    auto progress=origin,last_hello=origin-kRetry,last_finish=origin-kRetry,next_due=origin;
    std::optional<Clock::time_point> complete_since,stall_since;
    const auto send=[&](const std::vector<uint8_t>& b,bool retry=false) {
        if(socket.send(b)) { ++stats.sent; if(retry) ++stats.retransmits; }
    };
    const auto tagged=[&](Kind kind,uint32_t tick,uint64_t hash,bool retry=false) {
        std::vector<uint8_t> body; put(body,tick,4); put(body,hash,8); send(packet(o,kind,body),retry);
    };
    const auto hello=[&](bool retry=false) {
        std::vector<uint8_t> body; put(body,o.input_delay,4); send(packet(o,Kind::Hello,body),retry);
    };
    const auto finish_files=[&]() {
        if(record.is_open()) { record.seekp(16); stream_u32(record,lock.sim().tick()); stream_u32(record,recorded); }
        close_checked(record,"record"); close_checked(trace,"trace");
    };
    const auto end_stall=[&](Clock::time_point now) {
        if(stall_since) { stats.stall_ms+=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now-*stall_since).count()); stall_since.reset(); }
    };
    const auto mismatch=[&](uint32_t tick,uint64_t expected,uint64_t got) {
        stats.first_divergent_tick=std::min(stats.first_divergent_tick,tick);
        throw std::runtime_error("desync at tick "+std::to_string(tick)+" local_hash="+std::to_string(expected)+" peer_hash="+std::to_string(got));
    };
    const auto reconcile=[&]() {
        // Only executed, equal state hashes authorize a checksum acknowledgment.
        // A future checksum is buffered without asserting knowledge of future state.
        const uint32_t begin=stats.confirmed_ticks+1;
        for(uint32_t tick=begin;tick<=stats.tick;++tick) {
            if(!peer_hashes[tick]) continue;
            const auto hash=*checksums[tick].hash;
            if(*peer_hashes[tick]!=hash) mismatch(tick,hash,*peer_hashes[tick]);
            // An equal executed checksum also proves receipt of both input frames.
            auto& input=local[tick-1]; input.acked=true; input.wire.clear();
        }
        while(stats.confirmed_ticks<stats.tick) {
            const auto tick=stats.confirmed_ticks+1;
            if(!peer_hashes[tick] || !checksums[tick].acked) break;
            ++stats.confirmed_ticks; progress=Clock::now();
        }
    };
    const auto create_input=[&](uint32_t tick,uint32_t source,bool empty) {
        auto& input=local[tick];
        if(input.generated) throw std::runtime_error("duplicate local input scheduling");
        vf::TickFrame frame; frame.tick=tick; frame.player=static_cast<uint8_t>(o.player);
        if(!empty) {
            frame.commands=vf::make_ai_commands(lock.sim(),frame.player,sequence);
            for(auto& command:frame.commands) command.tick=tick;
        }
        input.created=Clock::now(); input.source_tick=source;
        const auto body=vf::serialize_frame(frame);
        input.digest=digest(body); input.wire=packet(o,Kind::Frame,body);
        if(lock.receive(frame)!=vf::ReceiveResult::Accepted) throw std::runtime_error("local scheduled frame rejected");
        input.frame=std::move(frame); input.generated=true; input.last_send=origin-kRetry;
    };
    try {
    while(true) {
        const auto now=Clock::now();
        if(o.exit_tick==stats.tick) throw std::runtime_error("injected disconnect at tick "+std::to_string(o.exit_tick));
        if(!complete_since && now-progress>std::chrono::milliseconds(o.timeout))
            throw std::runtime_error("timeout/disconnect waiting for "+std::string(!joined?"handshake":stats.tick==o.ticks?"all checksums/terminal confirmation":"missing input/checksum")+" at tick "+std::to_string(stats.tick));
        if(!peer_ready && now-last_hello>=kRetry) { hello(hello_sent); hello_sent=true; last_hello=now; }
        if(joined && !initialized) {
            for(uint32_t tick=0;tick<std::min(o.input_delay,o.ticks);++tick) create_input(tick,0,true);
            initialized=true; next_due=now;
        }
        if(initialized && stats.tick<o.ticks && now>=next_due && generated_source!=stats.tick) {
            // Sampling is tied to deterministic source state, never packet arrival.
            // It occurs at this tick's deadline before executing that tick.
            if(stats.tick+o.input_delay<o.ticks) create_input(stats.tick+o.input_delay,stats.tick,false);
            generated_source=stats.tick;
        }
        if(initialized) {
            uint32_t outstanding_inputs=0,outstanding_hashes=0;
            const auto first=stats.confirmed_ticks;
            const auto last=std::min(o.ticks,stats.tick+o.input_delay+1);
            for(uint32_t tick=first;tick<last;++tick) {
                auto& input=local[tick]; if(!input.generated || input.acked) continue;
                ++outstanding_inputs;
                if(now-input.last_send>=kRetry) { send(input.wire,input.sent); input.sent=true; input.last_send=now; }
            }
            for(uint32_t tick=first+1;tick<=stats.tick;++tick) {
                auto& checksum=checksums[tick]; if(checksum.acked) continue;
                ++outstanding_hashes;
                if(now-checksum.last_send>=kRetry) {
                    tagged(Kind::Checksum,tick,checksum.wire_hash,checksum.sent); checksum.sent=true; checksum.last_send=now;
                }
            }
            stats.max_unacked_frames=std::max(stats.max_unacked_frames,outstanding_inputs);
            stats.max_unacked_checksums=std::max(stats.max_unacked_checksums,outstanding_hashes);
            if(stats.tick==o.ticks && stats.confirmed_ticks==o.ticks && now-last_finish>=kRetry) {
                tagged(Kind::Finish,o.ticks,stats.hash,finish_sent); finish_sent=true; last_finish=now;
            }
        }
        // Bounded receive drain plus progress-based timeout defeats irrelevant traffic floods.
        for(unsigned drain=0;drain<256;++drain) {
            auto received=socket.receive(); if(!received) break;
            if(received->empty()) continue;
            ++stats.received;
            const auto bytes=std::span<const uint8_t>(*received);
            const auto kind=header(o,bytes); const auto body=bytes.subspan(kHeaderBytes);
            if(kind==Kind::Hello) {
                if(body.size()!=4 || get(body,0,4)!=o.input_delay) throw std::runtime_error("incompatible input delay");
                if(!joined) { joined=true; progress=Clock::now(); hello(); }
                else if(!peer_ready) hello();
                continue;
            }
            if(!joined) { hello(); continue; }
            peer_ready=true;
            if(kind==Kind::Frame) {
                vf::TickFrame frame;
                if(!vf::deserialize_frame(body,frame) || frame.player!=1-o.player || frame.tick>=o.ticks)
                    throw std::runtime_error("invalid peer tick frame");
                const auto hash=digest(body);
                if(remote_digests[frame.tick]) {
                    if(*remote_digests[frame.tick]!=hash) throw std::runtime_error("conflicting duplicate frame at tick "+std::to_string(frame.tick));
                    tagged(Kind::Ack,frame.tick,hash); continue;
                }
                if(frame.tick<stats.tick || frame.tick-stats.tick>o.input_delay+kVerificationLag)
                    throw std::runtime_error("peer frame exceeds bounded pipeline window");
                if(lock.receive(frame)!=vf::ReceiveResult::Accepted) throw std::runtime_error("peer frame rejected at tick "+std::to_string(frame.tick));
                remote_digests[frame.tick]=hash; remote[frame.tick]=std::move(frame);
                tagged(Kind::Ack,static_cast<uint32_t>(get(body,16,4)),hash);
            } else {
                if(body.size()!=12) throw std::runtime_error("invalid ACK/checksum/finish payload");
                const auto tick=static_cast<uint32_t>(get(body,0,4)); const auto hash=get(body,4,8);
                if(kind==Kind::Ack) {
                    if(tick>=o.ticks || !local[tick].generated) throw std::runtime_error("ACK for unsent tick");
                    auto& input=local[tick];
                    if(hash!=input.digest) throw std::runtime_error("ACK frame digest differs");
                    input.acked=true; input.receipt_acked=true; input.wire.clear();
                } else if(kind==Kind::Checksum || kind==Kind::ChecksumAck) {
                    if(tick==0 || tick>o.ticks) throw std::runtime_error("invalid checksum tick");
                    if(kind==Kind::ChecksumAck) {
                        auto& checksum=checksums[tick];
                        if(!checksum.hash || !checksum.sent) throw std::runtime_error("checksum ACK for unsent state");
                        if(hash!=checksum.wire_hash) throw std::runtime_error("checksum ACK hash differs");
                        checksum.acked=true;
                    } else {
                        if(tick>stats.tick && tick-stats.tick>kVerificationLag) throw std::runtime_error("peer checksum exceeds bounded verification window");
                        if(peer_hashes[tick] && *peer_hashes[tick]!=hash) throw std::runtime_error("conflicting duplicate checksum at tick "+std::to_string(tick));
                        peer_hashes[tick]=hash;
                        if(checksums[tick].hash) {
                            if(hash!=*checksums[tick].hash) mismatch(tick,*checksums[tick].hash,hash);
                            tagged(Kind::ChecksumAck,tick,hash);
                        }
                    }
                    reconcile();
                } else {
                    if(tick!=o.ticks) throw std::runtime_error("invalid terminal tick");
                    if(kind==Kind::Finish) {
                        if(tick>stats.tick && tick-stats.tick>kVerificationLag) throw std::runtime_error("premature terminal confirmation");
                        if(pending_finish && *pending_finish!=hash) throw std::runtime_error("conflicting terminal hash");
                        pending_finish=hash;
                        if(stats.tick==o.ticks && hash!=stats.hash) mismatch(tick,stats.hash,hash);
                    } else {
                        if(!finish_sent || stats.tick!=o.ticks) throw std::runtime_error("premature terminal acknowledgment");
                        if(hash!=stats.hash) mismatch(tick,stats.hash,hash);
                        if(!finish_acked) { finish_acked=true; progress=Clock::now(); }
                    }
                }
            }
        }
        const auto due_now=Clock::now();
        if(initialized && stats.tick<o.ticks && due_now>=next_due && generated_source==stats.tick) {
            const auto tick=stats.tick;
            const bool ready=local[tick].frame && remote[tick] && stats.tick-stats.confirmed_ticks<kVerificationLag;
            if(!ready) {
                if(!stall_since) { stall_since=due_now; ++stats.stalls; }
            } else {
                const bool resumed_after_stall=stall_since.has_value();
                end_stall(due_now);
                if(!local[tick].receipt_acked) ++stats.advanced_without_ack;
                if(lock.advance()!=vf::AdvanceResult::Advanced) throw std::runtime_error("invalid lockstep turn at tick "+std::to_string(tick));
                const auto executed=Clock::now();
                stats.tick=lock.sim().tick(); stats.hash=lock.sim().state_hash();
                const double executed_ms=milliseconds(executed-origin);
                if(!stats.tick_times_ms.empty()) stats.tick_interval_ms.push_back(executed_ms-stats.tick_times_ms.back());
                stats.tick_times_ms.push_back(executed_ms);
                stats.tick_deadline_ms.push_back(milliseconds(next_due-origin));
                stats.tick_lateness_ms.push_back(milliseconds(executed-next_due));
                stats.pacing_elapsed_ms=executed_ms-stats.tick_times_ms.front();
                for(uint32_t player=0;player<2;++player) {
                    const auto& frame=player==o.player?*local[tick].frame:*remote[tick];
                    for(const auto& command:frame.commands) {
                        if(player==o.player) {
                            const auto created_ms=milliseconds(local[tick].created-origin);
                            stats.command_latency_ms.push_back(executed_ms-created_ms); ++stats.command_count;
                            stats.command_timings.push_back({local[tick].source_tick,tick,command.sequence,created_ms,executed_ms});
                        }
                        if(record.is_open()) {
                            const auto b=vf::serialize_command(command);
                            if(recorded==100000 || record_bytes+4+b.size()>64*1024*1024) throw std::runtime_error("record exceeds VFR bounds");
                            stream_u32(record,static_cast<uint32_t>(b.size())); record.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
                            ++recorded; record_bytes+=4+b.size();
                        }
                    }
                }
                if(record.is_open() && !record) throw std::runtime_error("record write failed");
                if(trace.is_open()) { trace<<stats.tick<<' '<<stats.hash<<'\n'; if(!trace) throw std::runtime_error("trace write failed"); }
                local[tick].frame.reset(); remote[tick].reset();
                auto& checksum=checksums[stats.tick]; checksum.hash=stats.hash; checksum.wire_hash=stats.hash;
                if(stats.tick==o.desync) checksum.wire_hash^=1;
                checksum.last_send=origin-kRetry;
                if(peer_hashes[stats.tick]) {
                    if(*peer_hashes[stats.tick]!=stats.hash) mismatch(stats.tick,stats.hash,*peer_hashes[stats.tick]);
                    tagged(Kind::ChecksumAck,stats.tick,stats.hash);
                }
                reconcile();
                stats.max_verification_lag=std::max(stats.max_verification_lag,stats.tick-stats.confirmed_ticks);
                progress=executed;
                // Preserve the 20 Hz phase under ordinary polling jitter; this
                // can produce one shorter phase-correction interval. Reset after
                // a missing-data stall or a whole missed slot instead of catching up.
                next_due+=kTickPeriod;
                if(resumed_after_stall || next_due<=executed) next_due=executed+kTickPeriod;
            }
        }
        if(stats.tick==o.ticks && stats.confirmed_ticks==o.ticks && pending_finish) {
            if(*pending_finish!=stats.hash) mismatch(o.ticks,stats.hash,*pending_finish);
            if(!peer_finished) { peer_finished=true; progress=Clock::now(); }
            tagged(Kind::FinishAck,o.ticks,stats.hash);
            pending_finish.reset();
        }
        if(stats.tick==o.ticks && stats.confirmed_ticks==o.ticks && peer_finished && finish_acked && !complete_since)
            complete_since=Clock::now();
        // Full-timeout linger answers retried Finish and Checksum packets after loss.
        if(complete_since && Clock::now()-*complete_since>=std::chrono::milliseconds(o.timeout)) break;
        Sleep(1);
    }
    finish_files();
    } catch(...) {
        end_stall(Clock::now());
        // Failure evidence contains only the applied prefix, never scheduled future commands.
        try { finish_files(); }
        catch(const std::exception& cleanup_error) { std::cerr<<"evidence finalization error: "<<cleanup_error.what()<<'\n'; }
        throw;
    }
}
}
int main(int argc,char** argv) {
    const auto start=Clock::now(); Options options; Stats stats; bool parsed=false;
    try {
        options=parse(argc,argv); parsed=true; run(options,stats);
        report(options,stats,"complete","",start);
        std::cout<<"complete player="<<options.player<<" ticks="<<stats.tick<<" state_hash="<<stats.hash<<" sent="<<stats.sent<<" received="<<stats.received<<" retransmits="<<stats.retransmits<<'\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"peer error: "<<error.what()<<'\n';
        if(parsed) try { report(options,stats,"error",error.what(),start); }
        catch(const std::exception& report_error) { std::cerr<<"report error: "<<report_error.what()<<'\n'; }
        return 1;
    }
}
