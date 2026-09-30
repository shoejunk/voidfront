#include "voidfront_sim.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace vf;
namespace {
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
void steps(Sim& sim,int count) { while (count-->0) sim.step(); }
Unit& attacker(Sim& sim,uint32_t id,int x,int z,Order order=Order::Hold) {
    auto& u=const_cast<std::vector<Unit>&>(sim.units())[id-1];
    u.kind=UnitKind::Strider; u.hp=100; u.x=u.goal_x=u.next_x=x; u.z=u.goal_z=u.next_z=z;
    u.order=order; u.cooldown=0; u.path.clear(); u.detour.clear(); return u;
}
void remove_units(Sim& sim) { for (auto& u:const_cast<std::vector<Unit>&>(sim.units())) u.hp=0; }
void anchor_rules() {
    Sim sim(42,3,Map::Economy); remove_units(sim);
    check(sim.winner()==-1,"worker elimination incorrectly ended anchor match");
    const auto anchor=sim.structures()[1];
    auto& u=attacker(sim,1,anchor.x-3*kScale,anchor.z);
    sim.step(); check(sim.structures()[1].hp==992,"Hold did not fire at in-range structure");
    check(u.target_structure_id==anchor.id && u.target_id==0,"structure target namespace missing");
    auto changed=sim; const_cast<std::vector<Unit>&>(changed.units())[0].target_structure_id=0;
    check(sim.state_hash()!=changed.state_hash(),"structure target omitted from hash");
    u.order=Order::Move; u.cooldown=0; sim.step();
    check(sim.structures()[1].hp==992 && u.target_structure_id==0,"Move fired or retained structure target");
    u.order=Order::Hold; u.x=u.goal_x=anchor.x-5*kScale; u.cooldown=0;
    const auto x=u.x; sim.step();
    check(u.x==x && sim.structures()[1].hp==992,"Hold chased distant structure");
    check(sim.submit({sim.tick(),1,0,Order::AttackMove,{1},anchor.x,anchor.z}),"structure attack command rejected");
    steps(sim,1500);
    check(sim.winner()==0 && sim.structures()[1].hp==0,"AttackMove failed to destroy anchor");
    check(u.target_id==0 && u.target_structure_id==0 && !u.moving,"terminal retained attack/movement feedback");
    check(sim.units()[0].hp>0,"winning attacker died");
    Sim draw(42,3,Map::Economy); remove_units(draw);
    auto& anchors=const_cast<std::vector<Structure>&>(draw.structures());
    anchors[0].hp=anchors[1].hp=8;
    attacker(draw,1,anchors[1].x-3*kScale,anchors[1].z);
    attacker(draw,4,anchors[0].x+3*kScale,anchors[0].z);
    draw.step(); check(draw.winner()==2 && anchors[0].hp==0 && anchors[1].hp==0,"simultaneous anchor kills were not a draw");
    steps(draw,50); check(draw.winner()==2,"draw changed after terminal tick");
}
void target_priority() {
    Sim sim(42,3,Map::Economy); remove_units(sim);
    const auto anchor=sim.structures()[1];
    attacker(sim,1,anchor.x-3*kScale,anchor.z);
    auto& enemy=attacker(sim,4,anchor.x-3*kScale,anchor.z-kScale); enemy.kind=UnitKind::Worker;
    sim.step(); check(enemy.hp==92 && sim.structures()[1].hp==1000,"unit priority over structure failed");
    enemy.hp=0;
    auto& buildings=const_cast<std::vector<Structure>&>(sim.structures());
    // Equal footprint-distance fixture; source vector remains canonical ID order.
    buildings.push_back({3,1,StructureKind::Foundry,anchor.x-3*kScale,anchor.z+3*kScale,500});
    const_cast<std::vector<Unit>&>(sim.units())[0].cooldown=0;
    sim.step(); check(buildings[1].hp==992 && buildings[2].hp==500,"structure distance tie did not choose lowest ID");
}
Sim funded_factory() {
    Sim sim(42,3,Map::Economy); uint32_t sequence=0;
    for (int tick=0;tick<2000;++tick) {
        for (const auto& c:make_ai_commands(sim,0,sequence)) check(sim.submit(c),"factory setup AI rejected");
        sim.step();
        for (const auto& b:sim.structures()) if (b.player==0 && b.kind==StructureKind::Foundry && b.production_queue>0) return sim;
    }
    throw std::runtime_error("economic AI did not pay for production");
}
void destroyed_factory_and_navigation() {
    auto sim=funded_factory();
    auto& buildings=const_cast<std::vector<Structure>&>(sim.structures());
    auto& factory=buildings[2]; factory.hp=8;
    const auto center=nav::Point{factory.x,factory.z};
    check(sim.blocked(center.x/kScale,center.z/kScale),"live factory was not a blocker");
    check(sim.population_reserved(0)>0,"destruction fixture missing paid queue");
    remove_units(sim); attacker(sim,4,center.x+3*kScale,center.z);
    auto completion=sim;
    const_cast<std::vector<Structure>&>(completion.structures())[2].production_ticks=kProductionTicks-1;
    // Force the northwest exit. Its new Strider is outside the southeast
    // attacker's acquisition radius, so unit priority cannot redirect the shot.
    attacker(completion,4,center.x+3*kScale,center.z+3*kScale);
    attacker(completion,5,center.x-352,center.z).kind=UnitKind::Worker;
    completion.step();
    check(completion.structures()[2].hp==0 && completion.units().size()==sim.units().size()+1 &&
        completion.units().back().hp>0 && completion.population_reserved(0)==0,
        "production-before-combat lethal tick contract changed");
    const auto funds=sim.salvage(0); const auto roster=sim.units().size(); sim.step();
    check(factory.hp==0 && factory.production_queue==0 && factory.production_ticks==0 && !factory.spawn_blocked,
        "destroyed factory retained paid work");
    check(sim.salvage(0)==funds && sim.population_reserved(0)==0,"destruction refunded payment or retained reservation");
    check(!sim.blocked(center.x/kScale,center.z/kScale),"destruction left navigation blocker");
    check(sim.submit({sim.tick(),10000,0,Order::CancelProduction,{factory.id},0,0}),"dead factory receipt rejected");
    sim.step(); check(sim.command_result(0)==CommandResult::InvalidStructure && sim.salvage(0)==funds,"dead factory cancelled or refunded");
    check(sim.submit({sim.tick(),1,1,Order::Move,{4},center.x,center.z}),"ruin move rejected");
    steps(sim,100);
    check(sim.units()[3].x==center.x && sim.units()[3].z==center.z,"unit could not traverse destroyed footprint");
    check(sim.units().size()==roster,"destroyed factory spawned a unit");
}
void terminal_commands() {
    auto early=funded_factory(); remove_units(early);
    auto& buildings=const_cast<std::vector<Structure>&>(early.structures()); buildings[1].hp=8;
    attacker(early,1,buildings[1].x-3*kScale,buildings[1].z);
    auto late=early;
    const Command future{early.tick()+10,10000,0,Order::CancelProduction,{3},0,0};
    check(early.submit(future),"pre-terminal future command rejected");
    early.step(); late.step();
    check(early.winner()==0 && early.state_hash()==late.state_hash(),"terminal future arrival affected executed result");
    const auto balance=early.salvage(0); const auto queue=early.structures()[2].production_queue;
    const auto progress=early.structures()[2].production_ticks; const auto roster=early.units().size();
    for (int tick=0;tick<30;++tick) {
        if (tick==5) check(late.submit(future),"post-terminal receipt rejected");
        early.step(); late.step(); check(early.state_hash()==late.state_hash(),"terminal commands diverged by arrival time");
    }
    check(early.winner()==0 && early.salvage(0)==balance && early.structures()[2].production_queue==queue &&
        early.structures()[2].production_ticks==progress && early.units().size()==roster,"terminal command/economy mutated gameplay");
    uint32_t sequence=10000; check(make_ai_commands(early,0,sequence).empty(),"AI commanded finished match");
}
void economic_match(bool duel) {
    Sim sim(42,3,Map::Economy),replay(42,3,Map::Economy);
    std::array<uint32_t,2> sequences{}; std::array<int,9> command_counts{};
    uint32_t finish=0;
    // The 64x48 field increases travel and duel attrition; observed finish is ~12k ticks.
    for (int tick=0;tick<16000;++tick) {
        for (uint8_t p=duel?0:1;p<2;++p) for (const auto& c:make_ai_commands(sim,p,sequences[p])) {
            check(c.player==p,"AI used enemy command ownership");
            Command decoded; check(deserialize_command(serialize_command(c),decoded),"AI command wire roundtrip");
            check(sim.submit(c) && replay.submit(decoded),"economic AI command rejected"); ++command_counts[static_cast<int>(c.order)];
        }
        sim.step(); replay.step(); check(sim.state_hash()==replay.state_hash(),"economic command replay diverged");
        if (sim.winner()!=-1) { finish=sim.tick(); break; }
    }
    check(finish>0,"bounded economic match failed to finish");
    check(duel || sim.winner()==1,"economic AI failed to beat passive player");
    for (auto order:{Order::Gather,Order::Build,Order::TrainStrider,Order::AttackMove})
        check(command_counts[static_cast<int>(order)]>0,"AI skipped required economic command");
    check(sim.units().size()>6 && sim.structures().size()>(duel?3u:2u),"match did not produce/build");
    std::cout<<(duel?"AI duel":"AI vs passive")<<" winner="<<sim.winner()<<" tick="<<finish<<" hash="<<sim.state_hash()<<'\n';
}
}
int main() {
    try { anchor_rules(); target_priority(); destroyed_factory_and_navigation(); terminal_commands(); economic_match(false); economic_match(true);
        std::cout<<"economic match tests passed\n"; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
