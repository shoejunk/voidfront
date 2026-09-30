#include "voidfront_sim.hpp"
#include <iostream>
#include <stdexcept>
using namespace vf;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
 try {
    Sim sim(1,3,Map::Economy), replay(1,3,Map::Economy);
    check(sim.width()==64 && sim.height()==48,"map not enlarged");
    check(sim.visibility(0,4,12)==2 && sim.visibility(0,7,7)==2,"home economy lacks vision");
    check(sim.visibility(0,59,36)==0 && sim.visibility(1,4,12)==0,"enemy spawn leaked");
    check(sim.visibility(2,4,12)==0 && sim.visibility(0,-1,0)==0 && sim.visibility(0,64,0)==0,"vision bounds invalid");
    const auto command=[&](uint32_t sequence,int x,int z) {
        Command c{sim.tick(),sequence,0,Order::Move,{1},x*256+128,z*256+128};
        check(sim.submit(c) && replay.submit(c),"scout order rejected");
    };
    const auto advance=[&](int ticks) {
        while(ticks-->0) { sim.step(); replay.step(); check(sim.state_hash()==replay.state_hash(),"vision replay diverged"); }
    };
    command(1,26,24); advance(500);
    check(sim.units()[0].x==26*256+128 && sim.units()[0].z==24*256+128,"scout never arrived");
    check(sim.visibility(0,26,24)==2 && sim.visibility(1,26,24)==0,"scouting not team specific");
    command(2,6,11); advance(500);
    check(sim.visibility(0,26,24)==1,"exploration memory lost or stale visibility");
    Sim fresh(1,3,Map::Economy); check(fresh.visibility(0,26,24)==0,"reset retained exploration");
    // Destroy every home source: explored ground persists but loses live vision.
    for(auto& u:const_cast<std::vector<Unit>&>(sim.units())) if(u.player==0) u.hp=0;
    for(auto& b:const_cast<std::vector<Structure>&>(sim.structures())) if(b.player==0) b.hp=0;
    sim.step(); check(sim.visibility(0,4,12)==1,"dead source still reveals terrain");
    Sim legacy(1); check(legacy.visibility(0,25,12)==2,"legacy combat fog changed");
    std::cout<<"PASS: enlarged map, scout arrival, shared/team vision, exploration memory, dead sources, reset, replay\n";
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
