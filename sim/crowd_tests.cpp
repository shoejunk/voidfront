#include "voidfront_sim.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool ok,const char* why) { if (!ok) throw std::runtime_error(why); }
void order(vf::Sim& sim,uint32_t sequence,uint32_t id,int x,int z,vf::Order type=vf::Order::Move) {
    check(sim.submit({sim.tick(),sequence,sim.units()[id-1].player,type,{id},x,z}),"crowd command rejected");
}
void step(vf::Sim& sim,const std::string& name,std::ostream* trace) {
    const auto before=sim.units();
    sim.step();
    for (const auto& u:sim.units()) {
        const auto& old=before[u.id-1];
        const int64_t dx=u.x-old.x,dz=u.z-old.z;
        check(dx*dx+dz*dz<=1024,"crowd speed exceeded");
        for (const auto& v:sim.units()) if (v.id>u.id && u.hp>0 && v.hp>0)
            check(std::abs(u.x-v.x)>=128 || std::abs(u.z-v.z)>=128,"square crowd overlap");
        if (trace) *trace<<name<<','<<sim.tick()<<','<<u.id<<','<<u.x<<','<<u.z<<','<<u.hp<<','
            <<static_cast<int>(u.order)<<','<<u.target_id<<','<<u.detour.size()<<','<<u.blocked_ticks<<','<<sim.state_hash()<<'\n';
    }
}
void stationary(const std::string& name,uint32_t count,std::ostream* trace) {
    vf::Sim sim(1,count);
    const auto initial=sim.units();
    order(sim,1,1,640,4300);
    for (int i=0;i<400;++i) step(sim,name,trace);
    check(sim.units()[0].x==640 && sim.units()[0].z==4300,"stationary blockers prevented arrival");
    for (size_t i=1;i<initial.size();++i)
        check(sim.units()[i].x==initial[i].x && sim.units()[i].z==initial[i].z,"stationary blocker was pushed");
}
void swap(std::ostream* trace) {
    vf::Sim sim(1,2);
    order(sim,1,1,640,2688);
    order(sim,2,2,640,2432);
    for (int i=0;i<240;++i) step(sim,"opposing_swap",trace);
    check(sim.units()[0].x==640 && sim.units()[0].z==2688 &&
          sim.units()[1].x==640 && sim.units()[1].z==2432,"opposing swap permanently locked");
}
void moving_blocker(std::ostream* trace) {
    vf::Sim sim(1,2);
    order(sim,1,1,640,3500);
    for (int i=0;i<200;++i) {
        if (i==12) order(sim,2,2,2300,2688);
        step(sim,"moving_blocker",trace);
    }
    check(sim.units()[0].x==640 && sim.units()[0].z==3500,"moving blocker prevented arrival");
    check(sim.units()[1].x==2300 && sim.units()[1].z==2688,"moving blocker failed arrival");
}
void stop_and_retarget(std::ostream* trace) {
    vf::Sim sim(1,2);
    order(sim,1,1,640,3500);
    for (int i=0;i<80 && sim.units()[0].x==640;++i) step(sim,"detour_stop_retarget",trace);
    check(sim.units()[0].x!=640,"stop fixture never entered a detour");
    const auto at=sim.units()[0];
    order(sim,2,1,0,0,vf::Order::Stop);
    for (int i=0;i<40;++i) {
        step(sim,"detour_stop_retarget",trace);
        check(sim.units()[0].x==at.x && sim.units()[0].z==at.z,"detour survived Stop");
    }
    order(sim,3,1,2101,2117);
    for (int i=0;i<200;++i) step(sim,"detour_stop_retarget",trace);
    check(sim.units()[0].x==2101 && sim.units()[0].z==2117,"detour retained obsolete target");
}
void occupied_goal(std::ostream* trace) {
    vf::Sim sim(1,2);
    order(sim,1,1,640,2688);
    for (int i=0;i<160;++i) step(sim,"occupied_goal",trace);
    check(sim.units()[0].order==vf::Order::Move,"occupied goal reported arrival");
    check(sim.units()[1].x==640 && sim.units()[1].z==2688,"occupied goal moved its blocker");
    order(sim,2,2,2100,2688);
    for (int i=0;i<200;++i) step(sim,"occupied_goal",trace);
    check(sim.units()[0].x==640 && sim.units()[0].z==2688,"cleared goal remained unreachable");
}
void moving_attack_target(std::ostream* trace) {
    vf::Sim sim(1,2);
    order(sim,1,1,640,2432); // Suppress acquisition during setup.
    order(sim,2,2,900,2432);
    order(sim,1,3,2150,2530);
    for (int i=0;i<250;++i) step(sim,"moving_attack_target",trace);
    check(sim.units()[0].x==640 && sim.units()[1].x==900 && sim.units()[2].x==2150 &&
          sim.units()[2].z==2530,"moving-target fixture setup failed");
    order(sim,3,1,2150,3800,vf::Order::AttackMove);
    order(sim,2,3,2150,3800);
    bool detoured=false;
    for (int i=0;i<25;++i) {
        const auto enemy=sim.units()[2];
        step(sim,"moving_attack_target",trace);
        if (!sim.units()[0].detour.empty() && sim.units()[0].target_id==3 &&
            (enemy.x!=sim.units()[2].x || enemy.z!=sim.units()[2].z)) detoured=true;
    }
    check(detoured,"moving combat target continually invalidated avoidance");
}
}
int main(int argc,char** argv) {
    std::ofstream trace;
    if (argc==2) { trace.open(argv[1]); if (!trace) return 2; trace<<"case,tick,id,x,z,hp,order,target_id,detour_count,blocked_ticks,hash\n"; }
    auto* out=trace.is_open()? &trace:nullptr;
    int failed=0;
    const auto run=[&](const char* name,auto test) {
        try { test(); std::cout<<"PASS "<<name<<'\n'; }
        catch (const std::exception& e) { ++failed; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; }
    };
    run("stationary_blocker",[&] { stationary("stationary_blocker",2,out); });
    run("stationary_chain",[&] { stationary("stationary_chain",6,out); });
    run("opposing_swap",[&] { swap(out); });
    run("moving_blocker",[&] { moving_blocker(out); });
    run("detour_stop_retarget",[&] { stop_and_retarget(out); });
    run("occupied_goal",[&] { occupied_goal(out); });
    run("moving_attack_target",[&] { moving_attack_target(out); });
    return failed?1:0;
}
