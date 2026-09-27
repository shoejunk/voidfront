#include "voidfront_sim.hpp"
#include "lockstep.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace vf;
namespace {
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
void steps(Sim& s,int count) { while (count-->0) s.step(); }
Command order(const Sim& s,uint32_t seq,Order kind,uint32_t id) { return {s.tick(),seq,0,kind,{id},0,0}; }
void issue(Sim& s,uint32_t seq,Order kind,uint32_t id) {
    check(s.submit(order(s,seq,kind,id)),"production command submit"); s.step();
}
// A bounded funded-snapshot fixture: moving/returning is still executed through
// canonical commands, while cargo is injected only to isolate production rules.
Sim factory(int funds=1000) {
    Sim s(42,3,Map::Economy);
    auto& units=const_cast<std::vector<Unit>&>(s.units()); units[0].cargo=funds;
    issue(s,1,Order::ReturnCargo,1); steps(s,200);
    check(s.salvage(0)==funds,"fixture funding failed");
    const int x=4*kScale+128,z=4*kScale+128;
    check(s.can_build(0,x,z),"fixture site unavailable");
    check(s.submit({s.tick(),2,0,Order::Build,{1},x,z}),"fixture construction submit"); steps(s,400);
    check(s.structures().size()==3 && s.structures()[2].build_ticks==kBuildTicks,"fixture construction incomplete");
    return s;
}
void canonical_and_results() {
    Sim s(42,3,Map::Economy);
    auto c=order(s,1,Order::TrainStrider,1); Command decoded;
    check(deserialize_command(serialize_command(c),decoded) && decoded.order==Order::TrainStrider,"production wire roundtrip");
    c.x=1; check(serialize_command(c).empty() && !s.submit(c),"nonzero production coordinate accepted");
    c.x=0; c.units={1,2}; check(serialize_command(c).empty() && !s.submit(c),"multiple factory IDs accepted");
    c.units={2}; check(!s.submit(c),"enemy factory admitted");
    c.units={3}; check(!s.submit(c),"unit ID aliased absent structure");
    c.units={1}; check(s.submit(c),"anchor command receipt rejected"); s.step();
    check(s.command_result(0)==CommandResult::InvalidStructure,"anchor trained combat unit");
    Lockstep lock;
    c.tick=0; check(lock.receive({0,0,{c}})==ReceiveResult::Invalid,"combat lockstep accepted production ID alias");
    Sim combat; check(!combat.submit(c),"combat setup accepted production");
    auto f=factory(100);
    issue(f,3,Order::TrainStrider,3);
    check(f.command_result(0)==CommandResult::InsufficientSalvage && f.structures()[2].production_queue==0,"unfunded production changed queue");
    auto& b=const_cast<std::vector<Structure>&>(f.structures())[2]; b.build_ticks=99;
    issue(f,4,Order::TrainStrider,3);
    check(f.command_result(0)==CommandResult::NotReady,"unfinished factory trained");
    auto paid=factory(150);
    check(paid.submit(order(paid,3,Order::TrainStrider,3)) && paid.submit(order(paid,4,Order::TrainStrider,3)),"competing train receipts");
    paid.step();
    check(paid.salvage(0)==0 && paid.population_reserved(0)==1 && paid.command_result(0)==CommandResult::InsufficientSalvage && paid.result_sequence(0)==4,"same-tick train overdrew salvage or reordered");
}
void paid_queue_and_cancel() {
    auto s=factory(); const int balance=s.salvage(0);
    for (uint32_t seq=3;seq<9;++seq) check(s.submit(order(s,seq,Order::TrainStrider,3)),"batch receipt");
    s.step();
    check(s.structures()[2].production_queue==5 && s.structures()[2].production_ticks==1,"queue limit/order");
    check(s.salvage(0)==balance-250 && s.population_reserved(0)==5 && s.command_result(0)==CommandResult::QueueFull,"queue overcharge/reservation");
    steps(s,20); const auto progress=s.structures()[2].production_ticks;
    issue(s,9,Order::CancelProduction,3);
    check(s.structures()[2].production_queue==4 && s.structures()[2].production_ticks==progress+1 && s.salvage(0)==balance-200,"tail cancel reset front or lost refund");
    for (uint32_t seq=10;seq<14;++seq) check(s.submit(order(s,seq,Order::CancelProduction,3)),"cancel batch receipt");
    s.step();
    check(s.structures()[2].production_queue==0 && s.structures()[2].production_ticks==0 && s.population_reserved(0)==0 && s.salvage(0)==balance,"last cancel reset/refund");
    issue(s,14,Order::CancelProduction,3);
    check(s.command_result(0)==CommandResult::EmptyQueue && s.salvage(0)==balance,"empty cancellation refunded");
    issue(s,15,Order::TrainStrider,3); steps(s,98);
    check(s.units().size()==6 && s.structures()[2].production_ticks==99,"production finished early");
    s.step();
    check(s.units().size()==7 && s.units().back().id==7 && s.units().back().kind==UnitKind::Strider && s.population_used(0)==4 && s.population_reserved(0)==0,"production completion population/ID");
    const auto unit=s.units().back();
    check(s.submit({s.tick(),16,0,Order::Move,{7},unit.x-200,unit.z}),"produced unit not commandable"); steps(s,20);
    check(s.units().back().x==unit.x-200 && s.units().back().z==unit.z,"produced unit move failed");
}
void population_and_lifetime() {
    auto s=factory();
    // Two factories share the same player's reservation budget.
    auto& structures=const_cast<std::vector<Structure>&>(s.structures());
    auto second=structures[2]; second.id=4; second.x=10*kScale+128; second.z=14*kScale+128; structures.push_back(second);
    const int balance=s.salvage(0);
    for (uint32_t seq=3;seq<13;++seq) check(s.submit(order(s,seq,Order::TrainStrider,seq<8?3:4)),"population batch receipt");
    s.step();
    check(s.population_used(0)==3 && s.population_reserved(0)==9 && s.population_cap(0)==12 && s.salvage(0)==balance-450,"shared cap overcommitted");
    check(s.command_result(0)==CommandResult::PopulationFull,"shared population rejection missing");
    auto& units=const_cast<std::vector<Unit>&>(s.units()); units[1].hp=0;
    issue(s,13,Order::TrainStrider,4);
    check(s.population_reserved(0)==10 && s.population_used(0)==2,"dead unit still consumed population");
    auto lifetime=factory(); auto& roster=const_cast<std::vector<Unit>&>(lifetime.units());
    while (roster.size()<kLifetimeUnitLimit-1) { Unit dead; dead.id=static_cast<uint32_t>(roster.size())+1; dead.hp=0; roster.push_back(dead); }
    issue(lifetime,3,Order::TrainStrider,3); const int paid=lifetime.salvage(0);
    issue(lifetime,4,Order::TrainStrider,3);
    check(lifetime.command_result(0)==CommandResult::RosterFull && lifetime.salvage(0)==paid,"lifetime queue reservation failed");
    steps(lifetime,98);
    check(lifetime.units().size()==kLifetimeUnitLimit && lifetime.units().back().id==kLifetimeUnitLimit,"bounded monotonic spawn ID");
}
void blocked_spawn_and_hash() {
    auto s=factory(); issue(s,3,Order::TrainStrider,3);
    auto& units=const_cast<std::vector<Unit>&>(s.units());
    const auto b=s.structures()[2];
    constexpr nav::Point offsets[]={{-1,0},{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1}};
    // Isolate all eight exits with immutable worker blockers. These are injected
    // geometry fixtures, not gameplay population evidence.
    for (auto& u:units) u.hp=0;
    for (const auto d:offsets) {
        Unit u; u.id=static_cast<uint32_t>(units.size())+1; u.player=0; u.kind=UnitKind::Worker; u.order=Order::Hold;
        u.x=u.goal_x=u.next_x=b.x+d.x*352; u.z=u.goal_z=u.next_z=b.z+d.z*352; units.push_back(u);
    }
    steps(s,150); const auto before=s.units();
    check(s.structures()[2].spawn_blocked && s.structures()[2].production_ticks==100 && s.structures()[2].production_queue==1,"blocked spawn lost paid queue");
    auto altered=s;
    const_cast<std::vector<Structure>&>(altered.structures())[2].spawn_blocked=false;
    check(altered.state_hash()!=s.state_hash(),"spawn blocked omitted from hash");
    altered=s; const_cast<std::vector<Structure>&>(altered.structures())[2].production_ticks=99;
    check(altered.state_hash()!=s.state_hash(),"production ticks omitted from hash");
    altered=s; const_cast<std::vector<Structure>&>(altered.structures())[2].production_queue=2;
    check(altered.state_hash()!=s.state_hash(),"production queue omitted from hash");
    altered=s; const int paid=altered.salvage(0); issue(altered,4,Order::CancelProduction,3);
    check(!altered.structures()[2].spawn_blocked && altered.structures()[2].production_ticks==0 && altered.population_reserved(0)==0 && altered.salvage(0)==paid+kStriderCost,"blocked front cancellation lost reset/refund");
    const_cast<std::vector<Unit>&>(s.units())[6].hp=0; s.step();
    check(s.units().size()==before.size()+1 && !s.structures()[2].spawn_blocked && !s.population_reserved(0),"blocked exit did not recover");
    for (size_t i=0;i<before.size();++i) check(s.units()[i].x==before[i].x && s.units()[i].z==before[i].z,"spawn displaced blocker");
    const auto& spawned=s.units().back();
    for (const auto& other:s.units()) if (other.hp>0 && other.id!=spawned.id)
        check(std::abs(other.x-spawned.x)>=128 || std::abs(other.z-spawned.z)>=128,"spawn overlapped live unit");
}
void arrival_replay() {
    auto early=factory(),late=early;
    auto c=order(early,3,Order::TrainStrider,3); c.tick+=100;
    Command decoded; check(deserialize_command(serialize_command(c),decoded),"production replay decode");
    check(early.submit(decoded),"future production submit");
    for (int i=0;i<250;++i) {
        if (i==90) check(late.submit(decoded),"late production submit");
        early.step(); late.step(); check(early.state_hash()==late.state_hash(),"production arrival changed executed state");
    }
    check(early.units().size()==7 && early.hash()==late.hash(),"production replay did not spawn identically");
    std::cout<<"production deterministic hash "<<early.hash()<<'\n';
}
}
int main() {
    try { canonical_and_results(); paid_queue_and_cancel(); population_and_lifetime(); blocked_spawn_and_hash(); arrival_replay(); std::cout<<"production tests passed\n"; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
