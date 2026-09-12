#include "spatial.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        {
            vf::SpatialIndex boundary(128,128,256);
            boundary.insert(3,{32767,32767});
            boundary.insert(2,{256,256});
            boundary.insert(1,{0,0});
            std::vector<uint32_t> ids;
            boundary.query({-100,-100,32768,32768},ids);
            if (ids!=std::vector<uint32_t>{1,2,3}) throw std::runtime_error("boundary query lost ID ordering");
            for (const vf::nav::Rect r : {vf::nav::Rect{-10,-10,-1,-1},vf::nav::Rect{32768,0,33000,1},vf::nav::Rect{200,0,199,10}}) {
                boundary.query(r,ids);
                if (!ids.empty()) throw std::runtime_error("empty query retained entities");
            }
            // At 191 separation both full-speed sweeps penetrate; at 192 they
            // touch. Include both query extrema and the exact acquisition bound.
            for (const int sign : {-1,1}) {
                vf::SpatialIndex edge(128,128,256);
                const vf::nav::Point here{4096,4096};
                edge.insert(1,{here.x+sign*191,here.z});
                edge.insert(2,{here.x+sign*192,here.z});
                edge.insert(3,{here.x+sign*1568,here.z});
                edge.query({here.x-192,here.z-192,here.x+192,here.z+192},ids);
                if (ids!=std::vector<uint32_t>{1,2}) throw std::runtime_error("collision boundary omitted");
                edge.query({here.x-1568,here.z-1568,here.x+1568,here.z+1568},ids);
                if (ids!=std::vector<uint32_t>{1,2,3}) throw std::runtime_error("acquisition boundary omitted");
            }
        }
        uint32_t rng=42991;
        const auto random=[&]() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; };
        // Includes map extremes, bucket boundaries and prior moved-unit sweeps.
        for (int run=0;run<100;++run) {
            vf::SpatialIndex index(128,128,256);
            std::array<vf::nav::Point,500> start{},end{};
            for (size_t i=0;i<start.size();++i) {
                start[i]={int32_t(random()%32768),int32_t(random()%32768)};
                end[i]={start[i].x+int32_t(random()%65)-32,start[i].z+int32_t(random()%65)-32};
                index.insert(static_cast<uint32_t>(i+1),start[i]);
            }
            for (size_t i=0;i<start.size();++i) {
                std::vector<uint32_t> ids;
                const auto a=start[i],b=end[i];
                index.query({a.x-192,a.z-192,a.x+192,a.z+192},ids);
                if (!std::is_sorted(ids.begin(),ids.end()) || std::adjacent_find(ids.begin(),ids.end())!=ids.end())
                    throw std::runtime_error("spatial result not canonical");
                for (size_t j=0;j<start.size();++j) if (j!=i) {
                    const auto old=start[j],now=end[j];
                    vf::nav::Rect occupied{std::min(old.x,now.x)-128,std::min(old.z,now.z)-128,
                        std::max(old.x,now.x)+128,std::max(old.z,now.z)+128};
                    if (!vf::nav::segment_clear(a,b,occupied) &&
                        !std::binary_search(ids.begin(),ids.end(),static_cast<uint32_t>(j+1)))
                        throw std::runtime_error("broadphase missed swept collision");
                }
                constexpr int reach=1536+32;
                index.query({a.x-reach,a.z-reach,a.x+reach,a.z+reach},ids);
                for (size_t j=0;j<start.size();++j) {
                    const int64_t dx=int64_t(a.x)-end[j].x,dz=int64_t(a.z)-end[j].z;
                    if (dx*dx+dz*dz<=1536*1536 &&
                        !std::binary_search(ids.begin(),ids.end(),static_cast<uint32_t>(j+1)))
                        throw std::runtime_error("broadphase missed current target");
                }
            }
        }
        std::cout<<"PASS: 100 seeded 500-unit layouts, exhaustive swept collision and target coverage\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
