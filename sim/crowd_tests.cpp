#include "voidfront_sim.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool ok,const char* why) { if (!ok) throw std::runtime_error(why); }
struct Fraction { int64_t n,d; };
bool less(Fraction a,Fraction b) { return a.n*b.d<b.n*a.d; }
// Independent interval intersection for open square interiors. Relative motion
// checks the entire simultaneous interpolation, not just legal endpoints.
bool enters(int64_t x,int64_t z,int64_t end_x,int64_t end_z,
            int64_t min_x,int64_t min_z,int64_t max_x,int64_t max_z) {
    Fraction lower{0,1},upper{1,1};
    const auto axis=[&](int64_t start,int64_t end,int64_t minimum,int64_t maximum) {
        const auto delta=end-start;
        if (!delta) return minimum<start && start<maximum;
        Fraction a{minimum-start,delta},b{maximum-start,delta};
        if (delta<0) { a={start-maximum,-delta}; b={start-minimum,-delta}; }
        if (less(lower,a)) lower=a;
        if (less(b,upper)) upper=b;
        return less(lower,upper);
    };
    return axis(x,end_x,min_x,max_x) && axis(z,end_z,min_z,max_z) && less(lower,upper);
}
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
        for (const auto& r:vf::map_terrain(sim.map()))
            check(!enters(old.x,old.z,u.x,u.z,r.min_x-64,r.min_z-64,r.max_x+64,r.max_z+64),"crowd swept terrain collision");
        for (const auto& v:sim.units()) if (v.id>u.id && old.hp>0 && before[v.id-1].hp>0) {
            check(std::abs(u.x-v.x)>=128 || std::abs(u.z-v.z)>=128,"square crowd overlap");
            const auto& previous=before[v.id-1];
            check(!enters(int64_t(old.x)-previous.x,int64_t(old.z)-previous.z,
                int64_t(u.x)-v.x,int64_t(u.z)-v.z,-128,-128,128,128),"crowd relative swept overlap");
        }
        if (old.cooldown<=1 && u.cooldown==10)
            check(u.x==old.x && u.z==old.z,"firing unit moved later in its tick");
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

void short_disjoint(std::ostream* trace) {
    vf::Sim sim(1,2);
    order(sim,1,1,1024,2560);
    order(sim,2,2,1624,2560);
    for (int i=0;i<200;++i) step(sim,"short_disjoint",trace);
    check(sim.units()[0].x==1024 && sim.units()[0].z==2560 &&
        sim.units()[1].x==1624 && sim.units()[1].z==2560,"disjoint setup failed");
    order(sim,3,1,1124,2560);
    order(sim,4,2,1524,2560);
    for (int i=0;i<40;++i) {
        const auto before=sim.units();
        step(sim,"short_disjoint",trace);
        check(sim.units()[0].z==2560 && sim.units()[1].z==2560,"disjoint short journeys diverted sideways");
        check(sim.units()[0].x>=before[0].x && sim.units()[1].x<=before[1].x,"disjoint short journeys moved backward");
    }
    check(sim.units()[0].x==1124 && sim.units()[1].x==1524,"disjoint short goals not reached");
}

void firing_then_target_leaves(std::ostream* trace) {
    vf::Sim sim(1,1);
    uint32_t sequences[2]{1,1};
    order(sim,sequences[0]++,1,1024,2560);
    order(sim,sequences[1]++,2,1792,2560);
    for (int i=0;i<250;++i) {
        // Keep settled participants in explicit Move during setup, suppressing
        // combat without mutating simulator state or disabling a game rule.
        for (const auto& u:sim.units()) if (i>0 && u.x==u.goal_x && u.z==u.goal_z)
            order(sim,sequences[u.player]++,u.id,u.x,u.z);
        step(sim,"firing_then_target_leaves",trace);
    }
    check(sim.units()[0].x==1024 && sim.units()[0].z==2560 && sim.units()[1].x==1792 && sim.units()[1].z==2560,
        "firing departure setup failed");
    const auto shooter=sim.units()[0];
    order(sim,sequences[0]++,1,2304,2560,vf::Order::AttackMove);
    order(sim,sequences[1]++,2,2200,2560);
    step(sim,"firing_then_target_leaves",trace);
    check(sim.units()[1].hp==92 && sim.units()[0].cooldown==10 && sim.units()[0].target_id==2,
        "departure fixture did not fire before target moved");
    check(sim.units()[1].x>1792 && sim.units()[0].x==shooter.x && sim.units()[0].z==shooter.z,
        "shooter yielded after firing as target left range");
}

void opposed_pause_retarget(vf::Order pause,const std::string& name,std::ostream* trace) {
    vf::Sim sim(1,2);
    check(sim.submit({0,1,0,vf::Order::Move,{1,2},7000,3000}),"first crossing order rejected");
    check(sim.submit({0,1,1,vf::Order::Move,{3,4},1000,3000}),"opposed crossing order rejected");
    for (int i=0;i<80;++i) step(sim,name,trace);
    check(sim.units()[0].order==vf::Order::Move,"pause fixture already arrived");
    const auto stopped=sim.units()[0];
    order(sim,2,1,0,0,pause);
    for (int i=0;i<40;++i) {
        step(sim,name,trace);
        check(sim.units()[0].x==stopped.x && sim.units()[0].z==stopped.z && sim.units()[0].order==pause,
            "opposed traffic displaced explicit Stop/Hold");
    }
    order(sim,3,1,1301,2107);
    for (int i=0;i<300;++i) {
        step(sim,name,trace);
        check(sim.units()[0].goal_x==1301 && sim.units()[0].goal_z==2107,"recovery replaced retarget goal");
    }
    check(sim.units()[0].x==1301 && sim.units()[0].z==2107 && sim.units()[0].order==vf::Order::Stop,
        "retarget retained obsolete crossing route");
}

void outer_channel_prefix(std::ostream* trace) {
    vf::Sim sim(1,2);
    const int z[4]{400,600,400,600};
    for (uint32_t id=1;id<=4;++id) order(sim,(id-1)%2+1,id,id<=2?1200:6800,z[id-1]);
    for (int i=0;i<200;++i) step(sim,"outer_channel_prefix",trace);
    for (const auto& u:sim.units())
        check(u.x==(u.id<=2?1200:6800) && u.z==z[u.id-1],"outer channel setup failed");
    for (uint32_t id=1;id<=4;++id) order(sim,(id-1)%2+3,id,id<=2?6800:1200,z[id-1]);
    for (int i=0;i<80;++i) {
        step(sim,"outer_channel_prefix",trace);
        for (const auto& u:sim.units()) {
            check(u.z<=704,"outer channel route diverted into central portal");
            if (i<40) check(u.z==z[u.id-1],"distant outer-channel journeys diverted prematurely");
        }
    }
    // Prefix eligibility only. This does not claim completion after contact.
}

void follower_chain(int direction,const std::string& name,std::ostream* trace) {
    vf::Sim sim(1,6);
    for (uint32_t id=1;id<=6;++id) order(sim,id,id,640,2432+128*(id-1));
    for (int i=0;i<160;++i) step(sim,name,trace);
    for (const auto& u:sim.units()) if (u.id<=6)
        check(u.x==640 && u.z==2432+128*static_cast<int>(u.id-1),"follower chain setup failed");
    for (uint32_t id=1;id<=6;++id)
        order(sim,6+id,id,640,2432+128*static_cast<int>(id-1)+direction*512);
    bool synchronized=true;
    for (int i=1;i<=16;++i) {
        step(sim,name,trace);
        for (const auto& u:sim.units()) if (u.id<=6) {
            const int start=2432+128*static_cast<int>(u.id-1);
            check(u.goal_x==640 && u.goal_z==start+direction*512,"follower chain goal replaced");
            synchronized &= u.x==640 && u.z==start+direction*32*i;
        }
    }
    // Equal velocities keep every tangent pair separated for the full tick.
    // There is no obstacle, turn, merge or occupied destination requiring a
    // stop. Keep the complete failed ledger when this desired behavior fails.
    check(synchronized,"unobstructed tangent followers stopped or diverted");
    for (const auto& u:sim.units()) if (u.id<=6)
        check(u.order==vf::Order::Stop,"follower chain did not finish exactly");
}

void goal_redistribution(std::ostream* trace) {
    vf::Sim sim(1,6);
    for (uint32_t id=1;id<=6;++id) order(sim,id,id,640,2432+128*(id-1));
    for (int i=0;i<160;++i) step(sim,"goal_redistribution",trace);
    for (const auto& u:sim.units()) if (u.id<=6)
        check(u.x==640 && u.z==2432+128*static_cast<int>(u.id-1),"redistribution setup failed");
    for (uint32_t id=1;id<=6;++id) order(sim,6+id,id,2304,2432+128*(6-id));
    for (int i=0;i<600;++i) {
        step(sim,"goal_redistribution",trace);
        for (const auto& u:sim.units()) if (u.id<=6)
            check(u.goal_x==2304 && u.goal_z==2432+128*static_cast<int>(6-u.id),"redistribution goal replaced");
    }
    for (const auto& u:sim.units()) if (u.id<=6)
        check(u.x==u.goal_x && u.z==u.goal_z && u.order==vf::Order::Stop,"same-stream reversed goals remained locked");
}

void blocked_convoy(vf::Order pause,const std::string& name,std::ostream* trace) {
    vf::Sim sim(1,6);
    for (uint32_t id=1;id<=6;++id) order(sim,id,id,640,2432+128*(id-1));
    for (int i=0;i<160;++i) step(sim,name,trace);
    for (const auto& u:sim.units()) if (u.id<=6)
        check(u.x==640 && u.z==2432+128*static_cast<int>(u.id-1),"blocked convoy setup failed");
    for (uint32_t id=1;id<=5;++id) order(sim,6+id,id,640,2944+128*(id-1));
    order(sim,12,6,0,0,pause);
    for (int i=0;i<8;++i) {
        step(sim,name,trace);
        check(sim.units()[5].x==640 && sim.units()[5].z==3072 && sim.units()[5].order==pause,
            "blocked convoy displaced stationary leader");
        for (const auto& u:sim.units()) if (u.id<=5) {
            check(u.goal_x==640 && u.goal_z==2944+128*static_cast<int>(u.id-1),"blocked convoy goal replaced");
            if (i==0) check(u.x==640 && u.z==2432+128*static_cast<int>(u.id-1),
                "blocked convoy partially committed failed dependency");
        }
    }
    // Later local detours are allowed. This is rollback/immutability coverage,
    // not eventual progress through a permanently occupied destination.
}

void convoy_retarget(std::ostream* trace) {
    vf::Sim sim(1,6);
    for (uint32_t id=1;id<=6;++id) order(sim,id,id,640,2432+128*(id-1));
    for (int i=0;i<160;++i) step(sim,"convoy_retarget",trace);
    for (const auto& u:sim.units()) if (u.id<=6)
        check(u.x==640 && u.z==2432+128*static_cast<int>(u.id-1),"retarget convoy setup failed");
    for (uint32_t id=1;id<=6;++id) order(sim,6+id,id,640,2944+128*(id-1));
    for (int i=0;i<4;++i) step(sim,"convoy_retarget",trace);
    for (const auto& u:sim.units()) if (u.id<=6)
        check(u.x==640 && u.z==2560+128*static_cast<int>(u.id-1),"retarget convoy never advanced together");
    order(sim,13,3,1901,2816);
    for (int i=0;i<200;++i) {
        step(sim,"convoy_retarget",trace);
        if (i==0) check(sim.units()[2].x>640 && sim.units()[2].z==2816,"retarget convoy followed obsolete direction");
        for (const auto& u:sim.units()) if (u.id<=6)
            check(u.goal_x==(u.id==3?1901:640) && u.goal_z==(u.id==3?2816:2944+128*static_cast<int>(u.id-1)),
                "retarget convoy goal replaced");
    }
    for (const auto& u:sim.units()) if (u.id<=6)
        check(u.x==u.goal_x && u.z==u.goal_z && u.order==vf::Order::Stop,"retarget convoy failed exact arrival");
}
}
int main(int argc,char** argv) {
    std::ofstream trace;
    bool redistribution=false;
    const char* trace_path=nullptr;
    for (int i=1;i<argc;++i) {
        const std::string argument=argv[i];
        if (argument=="--cooperative") {} // Retained compatibility; coverage is now mandatory.
        else if (argument=="--redistribution") redistribution=true;
        else if (argument.starts_with("--") || trace_path) return 2;
        else trace_path=argv[i];
    }
    if (trace_path) { trace.open(trace_path); if (!trace) return 2; trace<<"case,tick,id,x,z,hp,order,target_id,detour_count,blocked_ticks,hash\n"; }
    auto* out=trace.is_open()? &trace:nullptr;
    int failed=0;
    const auto run=[&](const char* name,auto test) {
        try { test(); std::cout<<"PASS "<<name<<'\n'; }
        catch (const std::exception& e) { ++failed; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; }
    };
    run("swept_oracle",[&] {
        check(enters(128,112,112,128,-128,-128,128,128),"swept oracle missed corner crossing");
        check(!enters(128,112,128,144,-128,-128,128,128),"swept oracle rejected tangent");
        check(enters(0,0,0,0,-128,-128,128,128),"swept oracle missed stationary overlap");
    });
    run("stationary_blocker",[&] { stationary("stationary_blocker",2,out); });
    run("stationary_chain",[&] { stationary("stationary_chain",6,out); });
    run("opposing_swap",[&] { swap(out); });
    run("moving_blocker",[&] { moving_blocker(out); });
    run("detour_stop_retarget",[&] { stop_and_retarget(out); });
    run("occupied_goal",[&] { occupied_goal(out); });
    run("moving_attack_target",[&] { moving_attack_target(out); });
    run("short_disjoint",[&] { short_disjoint(out); });
    run("firing_then_target_leaves",[&] { firing_then_target_leaves(out); });
    run("opposed_stop_retarget",[&] { opposed_pause_retarget(vf::Order::Stop,"opposed_stop_retarget",out); });
    run("opposed_hold_retarget",[&] { opposed_pause_retarget(vf::Order::Hold,"opposed_hold_retarget",out); });
    run("outer_channel_prefix",[&] { outer_channel_prefix(out); });
    run("follower_chain_forward",[&] { follower_chain(1,"follower_chain_forward",out); });
    run("follower_chain_reverse",[&] { follower_chain(-1,"follower_chain_reverse",out); });
    run("convoy_stop",[&] { blocked_convoy(vf::Order::Stop,"convoy_stop",out); });
    run("convoy_hold",[&] { blocked_convoy(vf::Order::Hold,"convoy_hold",out); });
    run("convoy_retarget",[&] { convoy_retarget(out); });
    if (redistribution) run("goal_redistribution",[&] { goal_redistribution(out); });
    return failed?1:0;
}
