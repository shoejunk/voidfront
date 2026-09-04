// Loopback-only transport evidence harness; not a production Internet protocol.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "lockstep.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
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
enum class Kind : uint32_t { Hello=1, Frame=2, Ack=3, Finish=4, FinishAck=5 };
constexpr size_t kHeaderBytes=44, kMaxDatagram=60000;
constexpr auto kRetry = std::chrono::milliseconds(40);
struct Options {
    uint32_t player=2, port=0, remote_port=0, seed=42, count=6, ticks=2000, timeout=5000;
    uint32_t protocol=vf::kLockstepProtocolVersion, desync=UINT32_MAX, exit_tick=UINT32_MAX;
    uint64_t session=0, content=vf::kLockstepContentId;
    std::string trace, record, report;
};
struct Stats {
    uint64_t sent=0, received=0, retransmits=0, stalls=0, stall_ms=0;
    uint32_t tick=0;
    uint64_t hash=0;
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
    if((o.desync!=UINT32_MAX && o.desync>=o.ticks) || (o.exit_tick!=UINT32_MAX && o.exit_tick>o.ticks))
        throw std::invalid_argument("fault injection tick outside match");
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
    if(kind<1 || kind>5) throw std::runtime_error("unknown packet kind");
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
void report(const Options& o,const Stats& s,const std::string& status,const std::string& error,Clock::time_point start) {
    if(o.report.empty()) return;
    std::ofstream out(o.report); if(!out) throw std::runtime_error("report open failed");
    out<<"{\"status\":"<<json_string(status)<<",\"error\":"<<json_string(error)<<",\"player\":"<<o.player
       <<",\"ticks\":"<<s.tick<<",\"content_id\":\""<<o.content<<"\",\"final_hash\":\""<<s.hash<<"\",\"sent\":"<<s.sent<<",\"received\":"<<s.received
       <<",\"retransmits\":"<<s.retransmits<<",\"stall_count\":"<<s.stalls<<",\"stall_ms\":"<<s.stall_ms
       <<",\"elapsed_ms\":"<<std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count()<<"}\n";
    close_checked(out,"report");
}
void run(const Options& o,Stats& stats) {
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
    uint32_t recorded=0,sequence=0; size_t record_bytes=32;
    bool joined=false,local_acked=false,peer_finished=false,finish_acked=false,terminal=false;
    std::optional<vf::TickFrame> local_frame,remote_frame,future_remote_frame;
    std::optional<uint64_t> pending_finish_hash;
    std::vector<uint8_t> outgoing;
    std::vector<std::optional<uint64_t>> remote_digests(o.ticks);
    auto progress=Clock::now(),last_send=progress-kRetry;
    auto waiting_since=progress;
    std::optional<Clock::time_point> complete_since;
    bool sent_once=false;
    const auto send=[&](const std::vector<uint8_t>& b,bool retry=false) {
        if(socket.send(b)) { ++stats.sent; if(retry) ++stats.retransmits; }
    };
    const auto ack=[&](uint32_t tick,uint64_t hash,Kind kind) {
        std::vector<uint8_t> b; put(b,tick,4); put(b,hash,8); send(packet(o,kind,b));
    };
    const auto finish_files=[&]() {
        if(record.is_open()) {
            record.seekp(16); stream_u32(record,lock.sim().tick()); stream_u32(record,recorded);
        }
        close_checked(record,"record"); close_checked(trace,"trace");
    };
    try {
    while(true) {
        const auto now=Clock::now();
        if(o.exit_tick==lock.sim().tick()) throw std::runtime_error("injected disconnect at tick "+std::to_string(o.exit_tick));
        if(!complete_since && now-progress>std::chrono::milliseconds(o.timeout))
            throw std::runtime_error("timeout/disconnect waiting for "+std::string(!joined?"handshake":terminal?"terminal hash confirmation":"tick frames/ACK")+" at tick "+std::to_string(lock.sim().tick()));
        if(joined && !terminal && !local_frame) {
            vf::TickFrame frame; frame.tick=lock.sim().tick(); frame.player=static_cast<uint8_t>(o.player);
            frame.previous_hash=lock.sim().state_hash();
            frame.commands=vf::make_ai_commands(lock.sim(),frame.player,sequence);
            if(frame.tick==o.desync) frame.previous_hash^=1;
            if(lock.receive(frame)!=vf::ReceiveResult::Accepted) throw std::runtime_error("local frame rejected");
            local_frame=frame; outgoing=packet(o,Kind::Frame,vf::serialize_frame(frame));
            local_acked=false; sent_once=false; last_send=now-kRetry;
        }
        if(now-last_send>=kRetry) {
            if(!joined) send(packet(o,Kind::Hello),sent_once);
            else if(terminal) {
                std::vector<uint8_t> body; put(body,o.ticks,4); put(body,lock.sim().state_hash(),8);
                send(packet(o,Kind::Finish,body),sent_once);
            }
            else if(!local_acked) send(outgoing,sent_once);
            last_send=now; sent_once=true;
        }
        // A bounded drain leaves timers responsive even with malformed/flooded traffic.
        for(unsigned drain=0;drain<256;++drain) {
            auto received=socket.receive(); if(!received) break;
            if(received->empty()) continue;
            ++stats.received;
            const auto bytes=std::span<const uint8_t>(*received);
            const Kind kind=header(o,bytes); const auto body=bytes.subspan(kHeaderBytes);
            const bool first_packet=!joined;
            if(!joined) { joined=true; progress=Clock::now(); }
            if(kind==Kind::Hello) {
                if(!body.empty()) throw std::runtime_error("hello has unexpected payload");
                if(first_packet) send(packet(o,Kind::Hello));
            } else if(kind==Kind::Frame) {
                vf::TickFrame frame;
                if(!vf::deserialize_frame(body,frame) || frame.player!=1-o.player || frame.tick>=o.ticks)
                    throw std::runtime_error("invalid peer tick frame");
                const auto hash=digest(body);
                if(remote_digests[frame.tick]) {
                    if(*remote_digests[frame.tick]!=hash) throw std::runtime_error("conflicting duplicate frame at tick "+std::to_string(frame.tick));
                    ack(frame.tick,hash,Kind::Ack); continue;
                }
                if(frame.tick<lock.sim().tick() || frame.tick>lock.sim().tick()+1) throw std::runtime_error("peer frame exceeds stop-and-wait window");
                if(frame.tick==lock.sim().tick() && frame.previous_hash!=lock.sim().state_hash())
                    throw std::runtime_error("desync at tick "+std::to_string(frame.tick)+" local_hash="+std::to_string(lock.sim().state_hash())+" peer_hash="+std::to_string(frame.previous_hash));
                if(lock.receive(frame)!=vf::ReceiveResult::Accepted) throw std::runtime_error("peer frame rejected at tick "+std::to_string(frame.tick));
                remote_digests[frame.tick]=hash;
                if(frame.tick==lock.sim().tick()) remote_frame=frame; else future_remote_frame=frame;
                ack(frame.tick,hash,Kind::Ack);
            } else {
                if(body.size()!=12) throw std::runtime_error("invalid ACK/finish payload");
                const auto tick=static_cast<uint32_t>(get(body,0,4)); const auto hash=get(body,4,8);
                if(kind==Kind::Ack) {
                    if(local_frame && tick==local_frame->tick) {
                        if(hash!=digest(std::span<const uint8_t>(outgoing).subspan(kHeaderBytes))) throw std::runtime_error("ACK frame digest differs");
                        local_acked=true;
                    } else if(tick>=lock.sim().tick()) throw std::runtime_error("ACK for unsent tick");
                } else {
                    if(tick!=o.ticks) throw std::runtime_error("invalid terminal tick");
                    if(kind==Kind::Finish && lock.sim().tick()+1==o.ticks) {
                        if(pending_finish_hash && *pending_finish_hash!=hash) throw std::runtime_error("conflicting terminal hash");
                        pending_finish_hash=hash; continue;
                    }
                    if(lock.sim().tick()!=o.ticks) throw std::runtime_error("premature terminal confirmation");
                    if(hash!=lock.sim().state_hash()) throw std::runtime_error("terminal desync at tick "+std::to_string(tick)+" local_hash="+std::to_string(lock.sim().state_hash())+" peer_hash="+std::to_string(hash));
                    if(kind==Kind::Finish) { peer_finished=true; ack(tick,hash,Kind::FinishAck); }
                    else finish_acked=true;
                }
            }
        }
        if(!terminal && local_frame && remote_frame && local_acked) {
            if(lock.advance()!=vf::AdvanceResult::Advanced)
                throw std::runtime_error("lockstep failed/desynced advancing tick "+std::to_string(lock.sim().tick())+" local_hash="+std::to_string(lock.sim().state_hash())+" local_frame_hash="+std::to_string(local_frame->previous_hash)+" peer_hash="+std::to_string(remote_frame->previous_hash));
            if(record.is_open()) {
                for(uint32_t player=0;player<2;++player) {
                    const auto& frame=player==o.player?*local_frame:*remote_frame;
                    for(const auto& command:frame.commands) {
                        const auto b=vf::serialize_command(command);
                        if(recorded==100000 || record_bytes+4+b.size()>64*1024*1024) throw std::runtime_error("record exceeds VFR1 bounds");
                        stream_u32(record,static_cast<uint32_t>(b.size())); record.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
                        ++recorded; record_bytes+=4+b.size();
                    }
                }
                if(!record) throw std::runtime_error("record write failed");
            }
            stats.tick=lock.sim().tick(); stats.hash=lock.sim().state_hash();
            if(trace.is_open()) { trace<<stats.tick<<' '<<stats.hash<<'\n'; if(!trace) throw std::runtime_error("trace write failed"); }
            ++stats.stalls; stats.stall_ms+=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-waiting_since).count();
            progress=waiting_since=Clock::now(); local_frame.reset(); remote_frame=std::move(future_remote_frame); future_remote_frame.reset();
            terminal=stats.tick==o.ticks; last_send=progress-kRetry; sent_once=false;
            if(terminal && pending_finish_hash) {
                if(*pending_finish_hash!=stats.hash) throw std::runtime_error("terminal desync at tick "+std::to_string(stats.tick)+" local_hash="+std::to_string(stats.hash)+" peer_hash="+std::to_string(*pending_finish_hash));
                peer_finished=true; ack(o.ticks,stats.hash,Kind::FinishAck);
            }
        }
        if(terminal && peer_finished && finish_acked && !complete_since) complete_since=Clock::now();
        // Remain available for an entire peer timeout after confirmation, responding
        // to repeated Finish packets when a previous FinishAck was dropped.
        if(complete_since && Clock::now()-*complete_since>=std::chrono::milliseconds(o.timeout)) break;
        Sleep(1);
    }
    finish_files();
    } catch(...) {
        // Retain a replayable applied prefix on timeout/desync. A zero-tick
        // prefix is deliberately rejected by the replay loader as no match.
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
