// Loopback-only transport evidence harness; not a production Internet protocol.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include "session.hpp"
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
constexpr uint32_t kVerificationLag=vf::net::kVerificationLag;
using Stats = vf::net::SessionStats;
struct Options : vf::net::SessionOptions {
    std::string trace, record, report;
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
    vf::net::validate_session_options(o);
    if(aliases(o.trace,o.record) || aliases(o.trace,o.report) || aliases(o.record,o.report))
        throw std::invalid_argument("trace, record and report paths must differ");
    return o;
}
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
    std::array<char,4> bytes{};
    for(size_t i=0;i<bytes.size();++i) bytes[i]=static_cast<char>(n>>(8*i));
    s.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
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
    out<<",\"session_ready_ms\":";
    if(s.session_ready_ms) out<<*s.session_ready_ms; else out<<"null";
    out<<",\"startup_duration_ms\":";
    if(s.session_ready_ms) out<<*s.session_ready_ms; else out<<"null";
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
    TimerResolution timer_resolution;
    vf::net::Session session(o,vf::make_ai_commands);
    stats=session.stats();
    uint32_t recorded=0;
    size_t record_bytes=32;
    std::ofstream trace,record;
    if(!o.trace.empty()) { trace.open(o.trace); if(!trace) throw std::runtime_error("trace open failed"); }
    if(!o.record.empty()) {
        record.open(o.record,std::ios::binary); if(!record) throw std::runtime_error("record open failed");
        record.write("VFR\2",4); stream_u32(record,vf::kProtocolVersion); stream_u32(record,o.seed);
        stream_u32(record,o.count); stream_u32(record,o.ticks); stream_u32(record,0);
        stream_u32(record,static_cast<uint32_t>(vf::kLockstepContentId)); stream_u32(record,static_cast<uint32_t>(vf::kLockstepContentId>>32));
    }
    const auto finish_files=[&]() {
        if(record.is_open()) { record.seekp(16); stream_u32(record,session.sim().tick()); stream_u32(record,recorded); }
        close_checked(record,"record"); close_checked(trace,"trace");
    };
    try {
        while(true) {
            const auto result=session.poll();
            // Capture errors too: a checksum failure can follow this poll's
            // successfully executed tick, whose applied prefix must be saved.
            if(result.advanced) {
                for(const auto& frame:*session.applied_frames()) for(const auto& command:frame.commands) {
                    if(record.is_open()) {
                        const auto b=vf::serialize_command(command);
                        if(recorded==100000 || record_bytes+4+b.size()>64*1024*1024) throw std::runtime_error("record exceeds VFR bounds");
                        stream_u32(record,static_cast<uint32_t>(b.size())); record.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
                        ++recorded; record_bytes+=4+b.size();
                    }
                }
                if(record.is_open() && !record) throw std::runtime_error("record write failed");
                if(trace.is_open()) { trace<<session.stats().tick<<' '<<session.stats().hash<<'\n'; if(!trace) throw std::runtime_error("trace write failed"); }
            }
            if(result.status==vf::net::SessionStatus::Error) throw std::runtime_error(session.error());
            if(result.status==vf::net::SessionStatus::Complete) break;
            Sleep(1);
        }
        stats=session.stats();
        finish_files();
    } catch(...) {
        session.cancel();
        stats=session.stats();
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
