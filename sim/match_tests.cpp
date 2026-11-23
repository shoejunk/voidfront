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
    std::array<uint32_t,2> sequences{}; std::array<int,static_cast<size_t>(Order::TrainWorker)+1> command_counts{};
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
            if (wipe_roster && !recovered) {
                recovered=std::any_of(sim.units().begin()+wipe_roster,sim.units().end(),
                    [](const Unit& unit) { return unit.kind!=UnitKind::Worker && unit.hp>0; });
            }
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
// One worker remains after home depletion. A Hold Strider covers the approach
// to the stocked field. In the second fixture a friendly escort has sight and
// can clear that Strider. Without an escort, the guard leaves through fog.
void guarded_salvage(uint8_t player,bool escort,int carried=0) {
    Sim sim(42,3,Map::Economy);
    remove_units(sim);
    const int sign=player==0?1:-1;
    const uint32_t worker_id=3u*player+1,guard_id=3u*(1-player)+1;
    auto& units=const_cast<std::vector<Unit>&>(sim.units());
    units[worker_id-1].hp=100;
    const_cast<std::vector<Deposit>&>(sim.deposits())[player].remaining=0;
    const auto& site=sim.deposits()[1-player];
    attacker(sim,guard_id,site.x-sign*kScale,site.z-sign*2*kScale);
    if (escort) {
        // The miner is already committed. Keep the escort close enough to
        // observe the kill, then let the ordinary military AI take over.
        attacker(sim,worker_id,site.x-sign*4*kScale,site.z-sign*4*kScale).kind=UnitKind::Worker;
        attacker(sim,worker_id+1,site.x-sign*5*kScale,site.z-sign*4*kScale);
        units[guard_id-1].hp=24;
        units[worker_id-1].cargo=carried;
        check(sim.submit({0,1,player,Order::Gather,{worker_id},site.x,site.z}),"initial escorted Gather rejected");
    }
    sim.step();
    auto control=sim;
    // Identical unseen unit roster and position. Only the guard's ability
    // to threaten the field differs; it cannot inform a decision through fog.
    const_cast<std::vector<Unit>&>(control.units())[guard_id-1].kind=UnitKind::Worker;
    uint32_t sequence=escort?1:0,control_sequence=sequence;
    uint32_t sight=0,recall=0,departure=0,clear=0,retry=0,gather=0,mined=0,delivered=0,cargo_banked=0;
    for (int i=0;i<4000;++i) {
        const auto& guard=sim.units()[guard_id-1];
        const bool visible=sim.visibility(player,guard.x/kScale,guard.z/kScale)==2;
        if (!sight && visible) sight=sim.tick();
        if (!escort && recall && !departure && !visible && sim.tick()>recall+100) {
            const int x=site.x-sign*17*kScale, z=site.z+sign*3*kScale;
            check(sim.submit({sim.tick(),1,uint8_t(1-player),Order::Move,{guard_id},x,z}),"hidden guard withdrawal rejected");
            departure=sim.tick();
        }
        if (escort && !clear && guard.hp<=0) {
            check(visible,"escort lost sight of guard's defeat");
            clear=sim.tick();
        }
        const auto commands=make_ai_commands(sim,player,sequence);
        if (!sight) {
            const auto blind=make_ai_commands(control,player,control_sequence);
            check(blind.size()==commands.size(),"hidden defender changed the number of worker orders");
            for (size_t j=0;j<blind.size();++j) {
                check(serialize_command(blind[j])==serialize_command(commands[j]),"hidden defender changed order bytes");
                check(control.submit(blind[j]),"unarmed control rejected a command");
            }
            control.step();
        }
        for (const auto& c:commands) {
            if (c.units.front()==worker_id && c.order==Order::ReturnCargo && !recall) recall=sim.tick();
            if (sight && guard.hp>0 && visible) check(c.order!=Order::Gather && c.order!=Order::TrainWorker,
                "AI recruited or assigned an exposed miner to a guarded field");
            if (!escort && departure && !retry && (c.order==Order::Move || c.order==Order::Gather)) retry=sim.tick();
            if (sight && c.order==Order::Gather && c.x==site.x && c.z==site.z && !gather) gather=sim.tick();
            check(sim.submit(c),"defended field AI command rejected");
        }
        const int previous_cargo=sim.units()[worker_id-1].cargo,previous_bank=sim.salvage(player),previous_ore=site.remaining;
        sim.step();
        const auto& worker=sim.units()[worker_id-1];
        check(worker.hp>0,"AI lost its miner to the observed guard");
        if (!mined && previous_ore>site.remaining && worker.resource_id==site.id && worker.cargo_kind==0 && worker.cargo==previous_cargo+1) mined=sim.tick();
        if (previous_cargo>0 && worker.cargo==0 && sim.salvage(player)==previous_bank+previous_cargo) {
            if (carried && !cargo_banked && previous_cargo==carried) cargo_banked=sim.tick();
            if (mined) delivered=sim.tick();
        }
        if ((escort && gather && clear) || (!escort && delivered)) break;
    }
    check(sight && recall>=sight,"AI never recalled the worker from a defended alternate");
    if (carried) check(clear>recall && cargo_banked>clear && gather>=cargo_banked && gather<=cargo_banked+20,
        "clearance interrupted a partial cargo return or stranded the miner at home");
    else if (escort) check(clear>recall && gather>=clear && gather<=clear+20,
        "visible guard defeat failed to reopen the stocked field by the next decision");
    else check(departure>recall && retry>departure+100 && gather>=retry && mined>gather && delivered>mined,
        "unseen guard departure failed bounded re-scout, extraction or actual cash delivery");
    std::cout<<"guarded field player="<<int(player)<<" escort="<<escort<<" sight="<<sight<<" recall="<<recall
             <<" cargo_banked="<<cargo_banked<<" departure="<<departure<<" clear="<<clear<<" retry="<<retry<<" gather="<<gather
             <<" mined="<<mined<<" banked="<<delivered<<" hash="<<sim.state_hash()<<'\n';
}
// Six tiles behind the field is outside a stationary Strider's protected
// service area. The same observed position warrants a recall for a Lancer.
void far_side_guard(uint8_t player,UnitKind kind) {
    Sim sim(42,3,Map::Economy);
    remove_units(sim);
    const uint32_t worker_id=3u*player+1,guard_id=3u*(1-player)+1;
    const_cast<std::vector<Unit>&>(sim.units())[worker_id-1].hp=100;
    const_cast<std::vector<Deposit>&>(sim.deposits())[player].remaining=0;
    const auto& site=sim.deposits()[1-player];
    const int x=site.x+(player==0?6:-6)*kScale;
    attacker(sim,guard_id,x,site.z).kind=kind;
    uint32_t sequence=0,sight=0,recall=0,delivery=0;
    for (int i=0;i<2000;++i) {
        if (!sight && sim.visibility(player,x/kScale,site.z/kScale)==2) sight=sim.tick();
        for (const auto& c:make_ai_commands(sim,player,sequence)) {
            if (c.order==Order::ReturnCargo) recall=sim.tick();
            check(sim.submit(c),"far-side guard fixture rejected an AI command");
        }
        const int cargo=sim.units()[worker_id-1].cargo,bank=sim.salvage(player);
        sim.step();
        const auto& worker=sim.units()[worker_id-1];
        check(worker.hp==100,"far-side Hold guard damaged the miner");
        if (cargo && !worker.cargo && sim.salvage(player)==bank+cargo) delivery=sim.tick();
        if (delivery || recall) break;
    }
    if (kind==UnitKind::Strider) check(sight && !recall && delivery>sight && sim.salvage(player)==kCargoCapacity,
        "distant Hold Strider denied a safe full salvage delivery");
    else check(sight && recall>=sight && recall<=sight+20 && !delivery,
        "longer-range Lancer failed to trigger a timely recall at the same position");
    std::cout<<"far-side guard player="<<int(player)<<" kind="<<int(kind)<<" sight="<<sight
             <<" recall="<<recall<<" banked="<<delivery<<'\n';
}
// The same Strider on the near approach can Hold or intercept. The miner
// sees the position before either stance has produced a different action.
// Follow three whole trips: the route from the bank differs from the
// original scouting route, and the laden automatic return is a third leg.
void intercepted_salvage(uint8_t player,Order stance,bool withdraw,int reserve=2000) {
    Sim sim(42,3,Map::Economy);
    remove_units(sim);
    const int sign=player==0?1:-1;
    const uint32_t worker_id=3u*player+1,guard_id=3u*(1-player)+1;
    auto& worker=const_cast<std::vector<Unit>&>(sim.units())[worker_id-1];
    worker.hp=100;
    auto& deposits=const_cast<std::vector<Deposit>&>(sim.deposits());
    deposits[player].remaining=0;
    auto& site=deposits[1-player];
    site.remaining=reserve;
    const auto& guard=attacker(sim,guard_id,site.x,site.z-sign*6*kScale,stance);
    uint32_t sequence=0,sight=0,first_flank=0,first_delivery=0,last_delivery=0,departure=0;
    const int expected=std::min(reserve,3*kCargoCapacity);
    const int trips=(expected+kCargoCapacity-1)/kCargoCapacity;
    int payments=0,outbound=0,laden=0;
    for (int i=0;i<4500;++i) {
        const bool visible=sim.visibility(player,guard.x/kScale,guard.z/kScale)==2;
        if (visible && !sight) sight=sim.tick();
        if (withdraw && payments==1 && !departure) {
            check(!visible,"withdrawal fixture needs the guard out of sight");
            check(sim.submit({sim.tick(),1,uint8_t(1-player),Order::Move,{guard_id},
                site.x+sign*5*kScale,site.z+sign*5*kScale}),"hidden withdrawal rejected");
            departure=sim.tick();
        }
        for (const auto& c:make_ai_commands(sim,player,sequence)) {
            if (c.order==Order::Move && (c.x!=site.x || c.z!=site.z-sign*3*kScale)) {
                check(visible && !departure,"worker detoured around a hidden or departed guard");
                check(worker.order!=Order::Move || worker.goal_x!=c.x || worker.goal_z!=c.z,
                    "AI repeated the same active flank order");
                if (!first_flank) first_flank=sim.tick();
                if (worker.cargo) {
                    check(worker.cargo==kCargoCapacity || site.remaining==0,"flank interrupted a partial load");
                    ++laden;
                } else ++outbound;
            }
            check(sim.submit(c),"interception fixture rejected an economic command");
        }
        const auto bank=sim.salvage(player),cargo=worker.cargo;
        sim.step();
        check(worker.hp==100,"interceptor damaged the mining worker");
        if (sim.salvage(player)>bank) {
            check(cargo==std::min(kCargoCapacity,expected-bank) && !worker.cargo && sim.salvage(player)-bank==cargo,
                "flank worker failed to bank its complete load");
            if (!first_delivery) first_delivery=sim.tick();
            last_delivery=sim.tick();
            ++payments;
        }
        // A pursuer can follow a miner home and kill it after the deposit.
        if (payments==trips && (reserve>expected || sim.tick()-last_delivery>=200)) break;
    }
    check(sight && first_flank>=sight && first_delivery>first_flank && payments==trips &&
        site.remaining==reserve-expected && sim.salvage(player)==expected,
        "intercepted miner did not bank the harvested salvage");
    check(outbound==(withdraw?1:trips) && laden==outbound,"flank orders did not match the observed trips");
    std::cout<<"intercept player="<<int(player)<<" stance="<<int(stance)<<" reserve="<<reserve<<" withdrawn="<<departure
             <<" sight="<<sight<<" first_flank="<<first_flank<<" first_bank="<<first_delivery
             <<" final_bank="<<last_delivery<<" end="<<sim.tick()<<" deposits="<<payments<<" outbound="<<outbound<<" laden="<<laden
             <<" hp="<<worker.hp<<" hash="<<sim.state_hash()<<'\n';
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
    std::array<uint32_t,2> paid_army{};
    for (const auto& b:sim.structures()) if (b.kind==StructureKind::Foundry) paid_army[b.player]+=b.production_queue;
    const auto armies_rebuilt=[&]() {
        std::array<uint32_t,2> born{};
        for (size_t i=roster;i<sim.units().size();++i) {
            const auto& unit=sim.units()[i];
            if (unit.kind!=UnitKind::Worker) ++born[unit.player];
        }
        return born[0]>paid_army[0] && born[1]>paid_army[1];
    };
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
            armies_rebuilt()) break;
    }
    check(flux_returned[0] && flux_returned[1],"recovery discarded partial flux cargo");
    check(delivered[0] && delivered[1],"sole flux workers never resumed salvage income");
    check(armies_rebuilt(),"recovering economies failed to fund new military production");
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
    try { anchor_rules(); target_priority(); destroyed_factory_and_navigation(); terminal_commands();
        for (uint8_t p=0;p<2;++p) {
            guarded_salvage(p,false); guarded_salvage(p,true); guarded_salvage(p,true,3);
            far_side_guard(p,UnitKind::Strider); far_side_guard(p,UnitKind::Lancer);
            intercepted_salvage(p,Order::Hold,false); intercepted_salvage(p,Order::AttackMove,false);
            intercepted_salvage(p,Order::AttackMove,true);
            intercepted_salvage(p,Order::AttackMove,false,3); intercepted_salvage(p,Order::AttackMove,false,11);
        }
        lone_worker_recovery(); economic_match(false); duel_seed_survey(); economic_match(true); mirrored_economy();
        std::cout<<"economic match tests passed\n"; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
