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
    std::array<uint32_t,2> sequences{}; std::array<int,13> command_counts{};
    uint32_t finish=0;
    bool fielded_armies=false,recovered=false;
    size_t wipe_roster=0;
    // The 64x48 field increases travel and duel attrition; observed finish is ~12k ticks.
    for (int tick=0;tick<16000;++tick) {
        for (uint8_t p=duel?0:1;p<2;++p) for (const auto& c:make_ai_commands(sim,p,sequences[p])) {
            check(c.player==p,"AI used enemy command ownership");
            Command decoded; check(deserialize_command(serialize_command(c),decoded),"AI command wire roundtrip");
            check(sim.submit(c) && replay.submit(decoded),"economic AI command rejected"); ++command_counts[static_cast<int>(c.order)];
        }
        sim.step(); replay.step(); check(sim.state_hash()==replay.state_hash(),"economic command replay diverged");
        if (duel) {
            int army[2]{};
            for (const auto& u:sim.units()) if (u.hp>0 && u.kind!=UnitKind::Worker) ++army[u.player];
            fielded_armies=fielded_armies || (army[0]>0 && army[1]>0);
            if (fielded_armies && !wipe_roster && army[0]==0 && army[1]==0) wipe_roster=sim.units().size();
            if (wipe_roster && sim.units().size()>wipe_roster) recovered=true;
        }
        if (sim.winner()!=-1) { finish=sim.tick(); break; }
    }
    check(finish>0 || duel,"bounded economic match failed to finish");
    check(duel || sim.winner()==1,"economic AI failed to beat passive player");
    if (duel && finish==0) {
        // Mirrored duel: with first-strike and processing-order bias removed, the
        // armies can annihilate at the cap. That snapshot is not economic deadlock:
        // require actual replacement production after an earlier complete army wipe.
        check(wipe_roster && recovered,"mirrored AI did not rebuild after losing both armies");
        int army[2]{},structs[2]{}; for (const auto& u:sim.units()) if (u.hp>0 && u.kind!=UnitKind::Worker) ++army[u.player];
        for (const auto& b:sim.structures()) if (b.hp>0) ++structs[b.player];
        check(army[0]==army[1] && structs[0]==structs[1] && sim.salvage(0)==sim.salvage(1),"unresolved duel is not symmetric");
        std::cout<<"AI duel unresolved symmetric cap with recovery tick="<<sim.tick()<<" hash="<<sim.state_hash()<<'\n';
        return;
    }
    for (auto order:{Order::Gather,Order::Build,Order::TrainStrider,Order::AttackMove})
        check(command_counts[static_cast<int>(order)]>0,"AI skipped required economic command");
    check(sim.units().size()>6 && sim.structures().size()>(duel?3u:2u),"match did not produce/build");
    std::cout<<(duel?"AI duel":"AI vs passive")<<" winner="<<sim.winner()<<" tick="<<finish<<" hash="<<sim.state_hash()<<'\n';
}
void lone_worker_recovery() {
    Sim sim(42,3,Map::Economy);
    std::array<uint32_t,2> sequences{};
    bool funded=false;
    for (int tick=0;tick<2000 && !funded;++tick) {
        for (uint8_t p=0;p<2;++p) for (const auto& c:make_ai_commands(sim,p,sequences[p])) check(sim.submit(c),"recovery setup AI rejected");
        sim.step();
        bool queues[2]{};
        for (const auto& b:sim.structures()) if (b.kind==StructureKind::Foundry && b.production_queue) queues[b.player]=true;
        funded=queues[0] && queues[1];
    }
    check(funded && sim.salvage(0)<kStriderCost && sim.salvage(1)<kStriderCost,"recovery fixture not below replacement cost");
    // Controlled casualty fixture: only the first worker on each side survives,
    // already assigned to flux with a partial load. Keep banks and paid queues
    // intact; recovery must deliver new salvage and produce beyond those queues.
    for (auto& u:const_cast<std::vector<Unit>&>(sim.units())) {
        if (u.id!=1 && u.id!=4) { u.hp=0; continue; }
        u.order=Order::Gather; u.resource_id=u.player?4:3; u.build_id=0;
        u.cargo=5; u.cargo_kind=1; u.returning=false; u.work_ticks=0;
        u.goal_x=u.next_x=u.x; u.goal_z=u.next_z=u.z;
        u.path.clear(); u.detour.clear(); u.queue.clear();
    }
    const auto roster=sim.units().size();
    const auto existing_reservations=sim.population_reserved(0)+sim.population_reserved(1);
    const int initial_salvage[2]{sim.salvage(0),sim.salvage(1)},initial_flux[2]{sim.flux(0),sim.flux(1)};
    bool delivered[2]{},flux_returned[2]{};
    auto replay=sim;
    for (int tick=0;tick<4000;++tick) {
        for (uint8_t p=0;p<2;++p) for (const auto& c:make_ai_commands(sim,p,sequences[p])) {
            Command decoded; check(deserialize_command(serialize_command(c),decoded),"recovery command roundtrip failed");
            check(sim.submit(c) && replay.submit(decoded),"recovery AI command rejected");
        }
        sim.step(); replay.step();
        check(sim.state_hash()==replay.state_hash(),"recovery command replay diverged");
        for (int p=0;p<2;++p) {
            delivered[p]=delivered[p] || sim.salvage(p)>initial_salvage[p];
            flux_returned[p]=flux_returned[p] || sim.flux(p)>=initial_flux[p]+5;
        }
        const auto& a=sim.units()[0]; const auto& b=sim.units()[3];
        check(a.hp==b.hp && a.x==sim.width()*kScale-b.x && a.z==sim.height()*kScale-b.z &&
            a.order==b.order && a.cargo==b.cargo && a.cargo_kind==b.cargo_kind &&
            a.returning==b.returning && sim.salvage(0)==sim.salvage(1) && sim.flux(0)==sim.flux(1),
            "lone-worker recovery lost rotational symmetry");
        if (delivered[0] && delivered[1] && flux_returned[0] && flux_returned[1] &&
            sim.units().size()>roster+existing_reservations) break;
    }
    check(flux_returned[0] && flux_returned[1],"recovery discarded partial flux cargo");
    check(delivered[0] && delivered[1],"sole flux workers never resumed salvage income");
    check(sim.units().size()>roster+existing_reservations,"sole workers failed to fund replacement production");
}
// Bias survey: the same mirrored AI duel under several seeds. Reported, not
// asserted: it measures whether a side wins regardless of seed.
void duel_seed_survey() {
    std::array<int,3> wins{};
    for (uint32_t seed=1;seed<=8;++seed) {
        Sim sim(seed,3,Map::Economy); std::array<uint32_t,2> sequences{};
        for (int tick=0;tick<16000 && sim.winner()==-1;++tick) {
            for (uint8_t p=0;p<2;++p) for (const auto& c:make_ai_commands(sim,p,sequences[p])) check(sim.submit(c),"survey command rejected");
            sim.step();
        }
        if (seed==1) { int n[2]{},w[2]{},st[2]{}; for (const auto& u:sim.units()) if (u.hp>0) ++(u.kind==UnitKind::Worker?w:n)[u.player];
            for (const auto& b:sim.structures()) if (b.hp>0) ++st[b.player];
            std::cout<<"final army "<<n[0]<<"/"<<n[1]<<" workers "<<w[0]<<"/"<<w[1]<<" structures "<<st[0]<<"/"<<st[1]<<" salvage "<<sim.salvage(0)<<"/"<<sim.salvage(1)<<'\n'; }
        ++wins[sim.winner()<0?2:sim.winner()];
        std::cout<<"duel seed="<<seed<<" winner="<<sim.winner()<<" tick="<<sim.tick()<<'\n';
    }
    std::cout<<"duel survey P0="<<wins[0]<<" P1="<<wins[1]<<" none="<<wins[2]<<'\n';
}
// Mirror audit: until the opposing armies come within contact range, the two AI
// economies must be exact 180-degree rotations of each other (units, structures,
// orders, cargo and balances). Combat itself has a separate start-of-tick,
// rotationally covariant tie policy; this audit isolates the economy before contact.
void mirrored_economy() {
    Sim sim(42,3,Map::Economy); std::array<uint32_t,2> sequences{};
    const int32_t w=int32_t(sim.width())*kScale,h=int32_t(sim.height())*kScale;
    int audited=0;
    for (int tick=0;tick<3000;++tick) {
        for (uint8_t p=0;p<2;++p) for (const auto& c:make_ai_commands(sim,p,sequences[p])) check(sim.submit(c),"mirror AI rejected");
        sim.step();
        int64_t nearest=std::numeric_limits<int64_t>::max();
        for (const auto& a:sim.units()) for (const auto& b:sim.units()) if (a.hp>0 && b.hp>0 && a.player==0 && b.player==1) {
            const int64_t dx=a.x-b.x,dz=a.z-b.z; nearest=std::min(nearest,dx*dx+dz*dz);
        }
        if (nearest<int64_t(12*kScale)*(12*kScale)) break;
        using Row=std::array<int32_t,10>;
        std::array<std::vector<Row>,2> rows{};
        const auto rot=[&](const Unit& u,int32_t v,bool x) { return u.player?(x?w:h)-v:v; };
        for (const auto& u:sim.units()) rows[u.player].push_back({int32_t(u.kind),rot(u,u.x,true),rot(u,u.z,false),u.hp,int32_t(u.order),
            int32_t(u.cargo),rot(u,u.goal_x,true),rot(u,u.goal_z,false),int32_t(u.returning),int32_t(u.moving)});
        std::array<std::vector<Row>,2> st{};
        for (const auto& b:sim.structures()) st[b.player].push_back({int32_t(b.kind),(b.player?w-b.x:b.x),b.player?h-b.z:b.z,
            b.hp,int32_t(b.build_ticks),int32_t(b.production_queue),int32_t(b.production_ticks),int32_t(b.spawn_blocked),0,0});
        const bool same=rows[0]==rows[1] && st[0]==st[1] && sim.salvage(0)==sim.salvage(1) && sim.flux(0)==sim.flux(1);
        if (!same) { std::cout<<"mirror diverged tick="<<sim.tick()<<std::endl; check(false,"AI economy not rotationally symmetric before contact"); }
        ++audited;
    }
    check(audited>1000,"mirror audit ended before the economy matured");
}
}
int main() {
    try { anchor_rules(); target_priority(); destroyed_factory_and_navigation(); terminal_commands(); lone_worker_recovery(); economic_match(false); duel_seed_survey(); economic_match(true); mirrored_economy();
        std::cout<<"economic match tests passed\n"; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
