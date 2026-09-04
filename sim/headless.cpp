#include "voidfront_sim.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    // Wall-clock measurement is confined to this non-authoritative harness.
    uint32_t ticks = 2000, count = 6, seed = 42;
    std::string trace_path;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (i + 1 >= argc) throw std::invalid_argument("missing option value");
            if (arg == "--ticks") ticks = static_cast<uint32_t>(std::stoul(argv[++i]));
            else if (arg == "--units-per-team") count = static_cast<uint32_t>(std::stoul(argv[++i]));
            else if (arg == "--seed") seed = static_cast<uint32_t>(std::stoul(argv[++i]));
            else if (arg == "--trace") trace_path = argv[++i];
            else throw std::invalid_argument("unknown option");
        }
        if (ticks < 1 || ticks > 10000000) throw std::invalid_argument("ticks must be 1..10000000");
        vf::Sim sim(seed,count);
        std::array<uint32_t,2> sequence{};
        std::ofstream trace;
        if (!trace_path.empty()) { trace.open(trace_path); if(!trace) throw std::runtime_error("trace open failed"); }
        std::vector<int64_t> durations;
        std::vector<int64_t> active_durations;
        for (uint32_t tick = 0; tick < ticks; ++tick) {
            const bool active = sim.winner() == -1;
            const auto begin = std::chrono::steady_clock::now();
            for (uint8_t p = 0; p < 2; ++p) for (auto c : vf::make_ai_commands(sim,p,sequence[p]))
                if(!sim.submit(std::move(c))) throw std::runtime_error("AI command rejected");
            sim.step();
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();
            durations.push_back(elapsed);
            if (active) active_durations.push_back(elapsed);
            if(trace) trace << sim.tick() << ' ' << sim.hash() << '\n';
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
