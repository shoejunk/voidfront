#include "voidfront_sim.hpp"
#include "lockstep.hpp"
#include <iostream>
#include <stdexcept>
using namespace vf;
namespace {
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
void steps(Sim& s,int count) { while (count-->0) s.step(); }
void issue(Sim& s,uint32_t seq,Order kind,std::vector<uint32_t> ids,int32_t x=0,int32_t z=0) {
    check(s.submit({s.tick(),seq,0,kind,std::move(ids),x,z}),"command submit"); s.step();
}
auto& mutable_units(Sim& s) { return const_cast<std::vector<Unit>&>(s.units()); }
// Funded Foundry fixture. Resources are injected as carried cargo, then delivered
// through the ordinary ReturnCargo command so accounting paths stay exercised.
Sim foundry(uint8_t cargo_kind,int amount) {
    Sim s(42,3,Map::Economy);
    mutable_units(s)[0].cargo=200; issue(s,1,Order::ReturnCargo,{1}); steps(s,200);
    check(s.salvage(0)==200,"salvage fixture");
    check(s.submit({s.tick(),2,0,Order::Build,{1},4*kScale+128,4*kScale+128}),"foundry placement"); steps(s,400);
    check(s.structures().size()==3 && s.structures()[2].build_ticks==kBuildTicks,"foundry incomplete");
    mutable_units(s)[0].cargo=amount; mutable_units(s)[0].cargo_kind=cargo_kind;
    issue(s,3,Order::ReturnCargo,{1}); steps(s,200);
    return s;
}
void map_layout() {
    Sim s(1,3,Map::Economy);
    int flux=0;
    for (size_t i=0;i<s.deposits().size();++i) {
        check(s.deposits()[i].id==i+1,"deposit ids must index the vector");
        if (s.deposits()[i].kind==1) { ++flux; check(s.deposits()[i].remaining==1000,"flux amount"); }
    }
    check(flux==2 && s.deposits().size()==4,"expected two salvage and two flux deposits");
    check(s.flux(0)==0 && s.flux(1)==0 && !s.researched(0),"initial tech state");
}
void flux_gathering() {
    Sim s(42,3,Map::Economy);
    const auto& d=s.deposits()[2];
    check(d.kind==1,"deposit 3 is flux");
    issue(s,1,Order::Gather,{1},d.x,d.z);
    steps(s,1500);
    check(s.flux(0)>=10 && s.salvage(0)==0,"flux delivered to flux bank only");
    check(s.deposits()[2].remaining==1000-int(s.flux(0))-s.units()[0].cargo,"flux conserved");
}
void mixed_cargo_delivers_first() {
    Sim s(42,3,Map::Economy);
    mutable_units(s)[0].cargo=6; // salvage cargo
    const auto& d=s.deposits()[2];
    issue(s,1,Order::Gather,{1},d.x,d.z);
    check(s.units()[0].returning,"mixed cargo must return before switching");
    steps(s,300);
    check(s.salvage(0)==6 && s.flux(0)==0,"salvage cargo was credited as salvage");
    steps(s,1500);
    check(s.flux(0)>0,"worker resumed flux after delivery");
}
void research() {
    auto s=foundry(1,60);
    check(s.flux(0)==60,"flux fixture");
    // Spawn a Strider first so research can also reach existing units.
    issue(s,4,Order::TrainStrider,{3}); steps(s,110);
    check(s.units().size()==7 && s.units().back().hp==100,"unhardened strider");
    issue(s,5,Order::Research,{3});
    check(s.command_result(0)==CommandResult::Accepted && s.flux(0)==10 && s.research_ticks(0)>0,"research not charged");
    issue(s,6,Order::Research,{3});
    check(s.command_result(0)==CommandResult::AlreadyResearched && s.flux(0)==10,"duplicate research charged");
    steps(s,kResearchTicks);
    check(s.researched(0) && s.research_ticks(0)==0,"research did not finish");
    check(s.units().back().hp==150,"existing strider not hardened");
    check(s.units()[0].hp==100,"worker hardened");
    issue(s,7,Order::Research,{3});
    check(s.command_result(0)==CommandResult::AlreadyResearched,"finished research repurchased");
    issue(s,8,Order::TrainStrider,{3}); steps(s,100);
    check(s.units().back().hp==150 && s.units().back().kind==UnitKind::Strider && s.units().size()==8,"new strider not hardened");
    check(!s.researched(1),"enemy gained research");
}
void research_rules() {
    auto poor=foundry(1,30);
    issue(poor,4,Order::Research,{3});
    check(poor.command_result(0)==CommandResult::InsufficientFlux && poor.flux(0)==30 && !poor.research_ticks(0),"unfunded research charged");
    Sim early(42,3,Map::Economy);
    check(early.submit({0,1,0,Order::Research,{1},0,0}),"anchor receipt"); early.step();
    check(early.command_result(0)==CommandResult::InvalidStructure,"anchor researched");
    Command c{0,1,0,Order::Research,{3},1,0};
    check(!early.submit(c) && serialize_command(c).empty(),"research with coordinates accepted");
    Sim combat; check(!combat.submit({0,1,0,Order::Research,{1},0,0}),"combat map accepted research");
    Lockstep lock;
    check(lock.receive({0,0,{{0,1,0,Order::Research,{1},0,0}}})==ReceiveResult::Invalid,"lockstep combat accepted research");
}
void hash_covers_tech() {
    auto a=foundry(1,60); auto b=a;
    issue(a,4,Order::Research,{3}); issue(b,4,Order::Stop,{1});
    check(a.state_hash()!=b.state_hash(),"research omitted from hash");
    auto c=foundry(1,60),d=foundry(0,60);
    check(c.state_hash()!=d.state_hash(),"resource kind omitted from hash");
    Command decoded;
    check(deserialize_command(serialize_command({0,1,0,Order::Research,{3},0,0}),decoded) && decoded.order==Order::Research,"research wire roundtrip");
    // Replay equivalence: late arrival does not change executed state.
    auto early=foundry(1,60),late=early;
    Command r{early.tick()+20,4,0,Order::Research,{3},0,0};
    check(early.submit(r),"future research");
    for (int i=0;i<300;++i) { if (i==10) check(late.submit(r),"late research"); early.step(); late.step();
        check(early.state_hash()==late.state_hash(),"research arrival changed state"); }
    check(early.researched(0),"replayed research incomplete");
}
void ai_uses_flux() {
    Sim s(7,3,Map::Economy);
    std::array<uint32_t,2> seq{};
    int ai_research=0;
    for (int i=0;i<9000 && s.winner()==-1;++i) {
        for (uint8_t p=0;p<2;++p) for (const auto& c:make_ai_commands(s,p,seq[p])) {
            check(s.submit(c),"AI command rejected"); if (c.order==Order::Research) ++ai_research;
        }
        s.step();
    }
    std::cout<<"tech ai: tick "<<s.tick()<<" winner "<<s.winner()<<" flux "<<s.flux(0)<<'/'<<s.flux(1)
             <<" researched "<<s.researched(0)<<s.researched(1)<<" research cmds "<<ai_research<<'\n';
    check(ai_research>0 && (s.researched(0)||s.researched(1)),"AI never researched");
}
}
void lancer_rules() {
    auto s=foundry(1,100);
    check(s.salvage(0)==100 && s.flux(0)==100,"lancer fixture");
    issue(s,4,Order::TrainLancer,{3});
    check(s.command_result(0)==CommandResult::NotResearched && s.salvage(0)==100 && s.flux(0)==100,"lancer before research charged");
    issue(s,5,Order::Research,{3}); steps(s,kResearchTicks);
    check(s.researched(0) && s.flux(0)==50,"research for lancer fixture");
    issue(s,6,Order::TrainLancer,{3});
    check(s.command_result(0)==CommandResult::Accepted && s.salvage(0)==100-kLancerCost && s.flux(0)==50-kLancerFluxCost,"lancer not charged");
    check(s.structures()[2].queue_lancers==1,"queue slot not marked lancer");
    issue(s,7,Order::CancelProduction,{3});
    check(s.salvage(0)==100 && s.flux(0)==50 && s.structures()[2].queue_lancers==0 && s.structures()[2].production_queue==0,"lancer cancel refund");
    issue(s,8,Order::TrainLancer,{3});
    check(s.structures()[2].queue_lancers==1 && s.structures()[2].production_queue==1,"lancer queue");
    issue(s,9,Order::TrainStrider,{3});
    check(s.command_result(0)==CommandResult::InsufficientSalvage && s.structures()[2].production_queue==1,"strider unaffordable behind lancer");
    steps(s,kProductionTicks+5);
    const auto& l=s.units().back();
    check(l.kind==UnitKind::Lancer && l.hp==kLancerHp && s.structures()[2].queue_lancers==0,"lancer did not spawn");
    issue(s,10,Order::TrainLancer,{3});
    check(s.command_result(0)==CommandResult::InsufficientSalvage,"insufficient salvage must reject");
    // Lancers outrange Striders: at 4.5 tiles only the Lancer can hurt an enemy.
    Sim c(42,3,Map::Economy);
    auto& units=mutable_units(c);
    size_t enemy=0; for (size_t i=0;i<units.size();++i) if (units[i].player==1) { enemy=i; break; }
    Unit lancer=units[0]; lancer.id=static_cast<uint32_t>(units.size())+1; lancer.kind=UnitKind::Lancer; lancer.hp=kLancerHp;
    lancer.order=Order::Stop;
    const int32_t ex=units[enemy].x,ez=units[enemy].z;
    lancer.x=lancer.goal_x=lancer.next_x=ex-4*kScale-128; lancer.z=lancer.goal_z=lancer.next_z=ez;
    units.push_back(lancer);
    const int before=units[enemy].hp;
    c.step();
    check(c.units()[enemy].hp==before-kLancerDamage,"lancer failed to fire at 4.5 tiles");
}
int main() {
    try { map_layout(); flux_gathering(); mixed_cargo_delivers_first(); research(); research_rules(); hash_covers_tech(); ai_uses_flux(); lancer_rules();
        std::cout<<"tech tests passed\n"; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
