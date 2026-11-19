#include "voidfront_sim.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace vf;
namespace {
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
Command command(const Sim& s,uint32_t sequence,Order order,std::vector<uint32_t> units,int x=0,int z=0) { return {s.tick(),sequence,0,order,std::move(units),x,z}; }
int total(const Sim& s) {
    int result=s.salvage(0)+s.salvage(1)+s.flux(0)+s.flux(1);
    for (const auto& d:s.deposits()) result+=d.remaining;
    for (const auto& u:s.units()) result+=u.cargo;
    for (const auto& b:s.structures()) if (b.kind==StructureKind::Foundry) result+=kFoundryCost;
    return result;
}
void gather(Sim& s,uint32_t sequence=1,std::vector<uint32_t> workers={1,2,3}) {
    const auto d=s.deposits()[0];
    check(s.submit(command(s,sequence,Order::Gather,std::move(workers),d.x,d.z)),"gather submit");
}
void step(Sim& s,int count) { for (int i=0;i<count;++i) s.step(); }
void funded(Sim& s) {
    gather(s); step(s,4000);
    check(s.salvage(0)>=kFoundryCost,"workers did not return enough salvage");
    check(total(s)==6000,"mining failed conservation");
    check(s.submit(command(s,2,Order::Stop,{1,2,3})),"stop workers"); s.step();
}
nav::Point site(const Sim& s) {
    for (int z=3;z<21;++z) for (int x=3;x<14;++x) if (s.can_build(0,x*kScale+128,z*kScale+128)) return {x*kScale+128,z*kScale+128};
    throw std::runtime_error("no valid construction site");
}
void validation() {
    Sim s(42,6,Map::Economy);
    check(s.units().size()==6 && s.structures().size()==2 && s.deposits().size()==4,"initial economy entities");
    check(s.width()==64 && s.height()==48 && s.salvage(0)==0,"initial economy map/resources");
    auto c=command(s,1,Order::Gather,{4},s.deposits()[0].x,s.deposits()[0].z);
    check(!s.submit(c),"foreign gather accepted");
    c.units={1}; auto bytes=serialize_command(c); Command decoded;
    check(deserialize_command(bytes,decoded) && decoded.order==Order::Gather,"economy wire roundtrip");
    check(s.submit(c),"own gather accepted"); s.step(); check(!s.submit(c),"stale economy input accepted");
    const auto p=site(s);
    check(s.submit(command(s,2,Order::Build,{2},p.x,p.z)),"future semantic failure rejected at submit"); s.step();
    check(s.command_result(0)==CommandResult::InsufficientSalvage && s.structures().size()==2 && s.salvage(0)==0,"unfunded build changed state");
    check(!s.can_build(0,s.structures()[0].x,s.structures()[0].z),"anchor overlap accepted");
    check(!s.can_build(0,s.deposits()[0].x,s.deposits()[0].z),"deposit overlap accepted");
    check(!s.can_build(0,16*kScale,5*kScale),"terrain overlap accepted");
    check(!s.can_build(0,27*kScale,18*kScale),"enemy anchor linkage accepted");
    check(!s.can_build(0,0,0),"edge placement accepted");
    // Structures keep a two-cell worker lane: centers 3 cells from the anchor leave a one-cell gap.
    check(!s.can_build(0,s.structures()[0].x+3*kScale,s.structures()[0].z),"one-cell structure gap accepted");
    check(s.can_build(0,s.structures()[0].x+4*kScale,s.structures()[0].z),"two-cell structure gap rejected");
}
void interruption_and_depletion() {
    Sim s(42,3,Map::Economy); gather(s,1,{1});
    for (int i=0;i<500 && s.units()[0].cargo<3;++i) s.step();
    check(s.units()[0].cargo>0,"worker never extracted");
    const auto cargo=s.units()[0].cargo;
    check(s.submit(command(s,2,Order::Stop,{1})),"stop mining submit"); step(s,30);
    check(s.units()[0].cargo==cargo && s.salvage(0)==0,"stop discarded/deposited cargo");
    check(s.submit(command(s,3,Order::ReturnCargo,{1})),"explicit return submit"); step(s,500);
    check(s.units()[0].cargo==0 && s.salvage(0)==cargo && s.units()[0].order==Order::Stop,"explicit return did not terminate");
    check(total(s)==6000,"interruption conservation");
    // A small remaining reserve isolates competing final extraction without a long economic run.
    auto& deposits=const_cast<std::vector<Deposit>&>(s.deposits()); deposits[0].remaining=7;
    const int baseline=total(s); gather(s,4); step(s,1500);
    check(s.deposits()[0].remaining==0,"deposit did not deplete");
    check(total(s)==baseline,"simultaneous final extraction created salvage");
    check(std::all_of(s.units().begin(),s.units().begin()+3,[](const Unit& u){return u.cargo==0 && u.order==Order::Stop;}),"depleted workers did not return and stop");
}
void construction_and_routes() {
    Sim s(42,3,Map::Economy); funded(s); const auto p=site(s); const auto balance=s.salvage(0);
    check(s.submit(command(s,3,Order::Build,{1},p.x,p.z)),"build submit"); s.step();
    check(s.structures().size()==3 && s.salvage(0)==balance-kFoundryCost,"build debit/creation");
    check(s.structures().back().build_ticks==0,"construction teleported progress");
    check(s.submit(command(s,4,Order::Stop,{1})),"interrupt build submit"); step(s,150);
    check(s.structures().back().build_ticks==0,"unattended site constructed");
    check(s.submit(command(s,5,Order::Build,{1},p.x,p.z)),"resume build submit"); step(s,1500);
    check(s.structures().back().build_ticks==kBuildTicks && s.salvage(0)==balance-kFoundryCost,"resume failed or double charged");
    check(total(s)==6000,"construction conservation");
    check(!s.can_build(0,p.x,p.z),"duplicate completed site accepted");
    // March another worker across the newly blocked site, retaining full swept clearance.
    auto& units=const_cast<std::vector<Unit>&>(s.units());
    units[1].x=p.x-2*kScale; units[1].z=p.z; units[1].order=Order::Stop;
    const nav::Rect expanded{p.x-kScale-64,p.z-kScale-64,p.x+kScale+64,p.z+kScale+64};
    check(s.submit(command(s,6,Order::Move,{2},p.x+2*kScale,p.z)),"cross-foundation move submit");
    for (int i=0;i<600;++i) {
        const nav::Point before{units[1].x,units[1].z}; s.step();
        check(nav::segment_clear(before,{units[1].x,units[1].z},expanded),"dynamic structure crossed by unit sweep");
    }
    check(units[1].x==p.x+2*kScale && units[1].z==p.z,"worker did not route around foundation");
}
void occupied_service_goal() {
    Sim s(42,3,Map::Economy); gather(s,1,{1}); s.step();
    const nav::Point prior{s.units()[0].goal_x,s.units()[0].goal_z};
    // Freeze another friendly unit in the allocated service slot after the order.
    // Other slots remain accessible, so the harvester must recover on its own.
    auto& units=const_cast<std::vector<Unit>&>(s.units());
    units[1].x=prior.x; units[1].z=prior.z; units[1].order=Order::Hold;
    units[1].goal_x=prior.x; units[1].goal_z=prior.z;
    step(s,800);
    check(s.salvage(0)>0,"occupied harvest slot permanently stalled worker");
    check(units[1].x==prior.x && units[1].z==prior.z,"service recovery displaced held blocker");
    check(total(s)==6000,"service-slot recovery changed resource total");
}
void competing_purchases() {
    Sim s(42,3,Map::Economy);
    auto& reserves=const_cast<std::vector<Deposit>&>(s.deposits()); reserves[0].remaining=kFoundryCost;
    gather(s); step(s,4000);
    check(s.salvage(0)==kFoundryCost,"competing purchase fixture funding");
    const auto first=site(s);
    const nav::Point second{10*kScale+128,14*kScale+128};
    check(s.can_build(0,second.x,second.z),"second purchase fixture placement");
    const auto total_before=total(s);
    check(s.submit(command(s,2,Order::Build,{1},first.x,first.z)),"first purchase submit");
    check(s.submit(command(s,3,Order::Build,{2},second.x,second.z)),"competing purchase submit");
    s.step();
    check(s.structures().size()==3 && s.salvage(0)==0,"same-tick purchases overdrew salvage");
    check(s.structures().back().x==first.x && s.structures().back().z==first.z,"purchase command order was unstable");
    check(s.command_result(0)==CommandResult::InsufficientSalvage && s.result_sequence(0)==3,"competing purchase result missing");
    check(total(s)==total_before,"competing purchases violated conservation");
}
void replay_and_apply_time() {
    Sim early(91,3,Map::Economy),late(91,3,Map::Economy);
    gather(early); gather(late);
    auto future=command(early,2,Order::Build,{1},4*kScale+128,4*kScale+128); future.tick=1000;
    check(early.submit(future),"early future accepted");
    for (int t=0;t<1800;++t) {
        if (t==900) check(late.submit(future),"late future accepted");
        early.step(); late.step();
        check(early.state_hash()==late.state_hash(),"arrival time affected authoritative economy state");
    }
    check(early.hash()==late.hash(),"economy replay diverged");
    std::cout<<"economy deterministic hash "<<early.hash()<<'\n';
}

void worker_replacement(int survivors,int prepaid,int starting_salvage) {
    Sim sim(42,3,Map::Economy);
    std::array<uint32_t,2> sequences{};
    // Mine the bank with one worker on each side. Preserve these balances and any
    // paid anchor work when applying the same casualties to the rotated bases.
    for (uint8_t p=0;p<2;++p) {
        const auto& mine=sim.deposits()[p];
        check(sim.submit({sim.tick(),++sequences[p],p,Order::Gather,{3u*p+1},mine.x,mine.z}),"replacement funding order failed");
    }
    while (sim.salvage(0)<starting_salvage && sim.tick()<8000) sim.step();
    for (uint8_t p=0;p<2;++p) {
        check(sim.salvage(p)==starting_salvage,"replacement fixture did not earn the starting bank");
        for (int i=0;i<prepaid;++i)
            check(sim.submit({sim.tick(),++sequences[p],p,Order::TrainWorker,{sim.structures()[p].id}}),"prepaid worker order failed");
    }
    const size_t starting_roster=sim.units().size();
    for (auto& u:const_cast<std::vector<Unit>&>(sim.units()))
        if (!survivors || u.id!=3u*u.player+2) u.hp=0;
    sim.step();
    for (uint8_t p=0;p<2;++p)
        check(sim.population_used(p)==survivors && sim.structures()[p].production_queue==prepaid &&
            sim.salvage(p)==starting_salvage-prepaid*kWorkerCost,"casualties changed the bank or paid anchor work");

    auto replay=sim;
    std::array<int,2> purchases{};
    std::array<bool,2> recruit_delivered{};
    const bool stranded=survivors==0 && prepaid==0 && starting_salvage<kWorkerCost;
    for (int tick=0;tick<(stranded?200:3500);++tick) {
        std::array<uint32_t,2> returning_recruit{};
        for (const auto& u:sim.units())
            if (u.id>starting_roster && u.kind==UnitKind::Worker && u.order==Order::Gather &&
                u.returning && u.cargo>0 && u.cargo_kind==0) returning_recruit[u.player]=u.id;
        const std::array<int,2> previous_bank{sim.salvage(0),sim.salvage(1)};
        for (uint8_t p=0;p<2;++p) {
            int planned_spending=0;
            for (const auto& c:make_ai_commands(sim,p,sequences[p])) {
                if (c.order==Order::TrainWorker) {
                    check(previous_bank[p]>=kWorkerCost,"AI ordered an unfunded worker");
                    check(++purchases[p]<=3-survivors-prepaid,"AI purchased workers already alive or reserved");
                    check(c.units==std::vector<uint32_t>{sim.structures()[p].id},"AI trained at the wrong anchor");
                    planned_spending+=kWorkerCost;
                } else if (c.order==Order::Build) {
                    const bool resuming=std::any_of(sim.structures().begin(),sim.structures().end(),[&](const Structure& b) {
                        return b.player==p && b.hp>0 && b.kind==StructureKind::Foundry && b.x==c.x && b.z==c.z;
                    });
                    if (!resuming) planned_spending+=kFoundryCost;
                } else if (c.order==Order::TrainStrider || c.order==Order::TrainLancer) {
                    planned_spending+=c.order==Order::TrainStrider?kStriderCost:kLancerCost;
                }
                Command decoded;
                check(c.player==p && deserialize_command(serialize_command(c),decoded) &&
                    sim.submit(c) && replay.submit(decoded),"replacement command wire replay failed");
            }
            check(planned_spending<=previous_bank[p],"AI allocated the same salvage to multiple purchases");
        }
        sim.step(); replay.step();
        check(sim.state_hash()==replay.state_hash(),"worker replacement replay diverged");
        check(purchases[0]==purchases[1] && sim.salvage(0)==sim.salvage(1) &&
            sim.structures()[0].production_queue==sim.structures()[1].production_queue,
            "mirrored worker replacement diverged");
        for (uint8_t p=0;p<2;++p) {
            const auto id=returning_recruit[p];
            if (id && sim.units()[id-1].cargo==0 && sim.salvage(p)>previous_bank[p]) recruit_delivered[p]=true;
        }
        if (recruit_delivered[0] && recruit_delivered[1] && purchases[0]==3-survivors-prepaid &&
            sim.structures()[0].production_queue==0) break;
    }
    for (uint8_t p=0;p<2;++p) {
        if (stranded) {
            check(!purchases[p] && sim.units().size()==starting_roster && sim.salvage(p)==starting_salvage,
                "unfunded, wiped economy created workers or income");
        } else {
            const auto living=std::count_if(sim.units().begin(),sim.units().end(),[p](const Unit& u) {
                return u.player==p && u.hp>0 && u.kind==UnitKind::Worker;
            });
            check(living==3 && purchases[p]==3-survivors-prepaid && !sim.structures()[p].production_queue,
                "funded AI failed to restore three workers through the anchor queue");
            check(recruit_delivered[p],"AI recruit never mined and delivered salvage");
        }
    }
    std::cout<<"worker recovery survivors="<<survivors<<" prepaid="<<prepaid<<" bank="<<starting_salvage
             <<" purchases="<<purchases[0]<<"/"<<purchases[1]<<" tick="<<sim.tick()<<" hash="<<sim.state_hash()<<'\n';
}
}
int main() {
    try {
        validation(); interruption_and_depletion(); construction_and_routes(); occupied_service_goal(); competing_purchases(); replay_and_apply_time();
        worker_replacement(0,0,kWorkerCost);
        worker_replacement(1,1,3*kWorkerCost);
        worker_replacement(1,0,kWorkerCost-kCargoCapacity);
        worker_replacement(0,0,kWorkerCost-kCargoCapacity);
        std::cout<<"economy tests passed\n";
    }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
