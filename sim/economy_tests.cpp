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

// There are two salvage deposits on the Economy map. Test each orientation
// separately so the other anchor's deposit can still hold salvage after we
// exhaust our own. Enemy workers stay at their starting positions.
enum class PriorSight { Unseen, Stocked, Empty };
Sim depleted_base(uint8_t player,int survivors,PriorSight prior,bool foundry,uint32_t& sequence,int remnant=0,bool paid_army=false) {
    Sim sim(42,3,Map::Economy);
    auto& deposits=const_cast<std::vector<Deposit>&>(sim.deposits());
    auto& home=deposits[player];
    auto& remote=deposits[1-player];
    const int target_bank=kWorkerCost+(foundry?kFoundryCost:0)+(paid_army?kStriderCost:0);
    home.remaining=target_bank+remnant;
    remote.remaining=prior==PriorSight::Empty?0:100;
    const uint32_t miner=3u*player+1;
    check(sim.submit({sim.tick(),++sequence,player,Order::Gather,{miner},home.x,home.z}),"finite map funding order failed");
    if (prior!=PriorSight::Unseen) {
        const int side=player==0?-1:1;
        check(sim.submit({sim.tick(),++sequence,player,Order::Move,{miner+1},remote.x+side*2*kScale,remote.z}),
            "preliminary scout order failed");
    }
    const auto sight=[&]{ return sim.visibility(player,remote.x/kScale,remote.z/kScale); };
    for (int i=0;i<8000 && (sim.salvage(player)<target_bank || (prior!=PriorSight::Unseen && sight()!=2));++i) sim.step();
    check(home.remaining==remnant && sim.salvage(player)==target_bank,"finite map did not earn its opening bank");
    if (remnant) check(sim.submit({sim.tick(),++sequence,player,Order::Stop,{miner}}),"remnant fixture stop rejected");
    check(sight()==(prior==PriorSight::Unseen?0:2),"fixture did not establish the requested scouting history");
    if (foundry) {
        const auto& anchor=sim.structures()[player];
        const int side=player==0?1:-1;
        check(sim.submit({sim.tick(),++sequence,player,Order::Build,{miner+2},
            anchor.x+side*5*kScale,anchor.z+side*3*kScale}),"finite map foundry order failed");
        step(sim,500);
        check(sim.structures().size()==3 && sim.structures().back().build_ticks==kBuildTicks,"finite map foundry unavailable");
        if (paid_army) check(sim.submit({sim.tick(),++sequence,player,Order::TrainStrider,{sim.structures().back().id}}),
            "paid scout fixture failed to enqueue Strider");
    }
    for (auto& u:const_cast<std::vector<Unit>&>(sim.units()))
        if (u.player==player && u.id>=miner+survivors) u.hp=0;
    do { sim.step(); } while (sim.tick()%20);
    check(sim.salvage(player)==kWorkerCost && sight()==(prior==PriorSight::Unseen?0:1),
        "finite map must start with last worker cost and remote site outside current sight");
    return sim;
}

struct SearchTimes {
    uint32_t sight=0,mine=0,delivery=0;
    bool operator==(const SearchTimes&) const = default;
};
SearchTimes finite_salvage_search(uint8_t player,bool survivor,PriorSight prior) {
    uint32_t sequence=0;
    auto sim=depleted_base(player,survivor,prior,false,sequence);
    auto replay=sim;
    auto dry=sim;
    uint32_t dry_sequence=sequence;
    // A hidden opponent could have emptied the patch after the scout died.
    // Both worlds must produce byte-identical decisions until sight returns.
    const uint32_t remote_index=1-player;
    const_cast<std::vector<Deposit>&>(dry.deposits())[remote_index].remaining=0;
    const auto& patch=sim.deposits()[remote_index];
    const auto sight=[&](const Sim& s){ return s.visibility(player,patch.x/kScale,patch.z/kScale); };
    SearchTimes times;
    uint32_t first_miner=0;
    int search_moves=0,search_recruits=0;
    bool bought_at_patch=false;
    for (int i=0;i<5000 && !times.delivery;++i) {
        const bool searching=!times.sight && sight(sim)<2;
        auto commands=make_ai_commands(sim,player,sequence);
        if (searching) {
            check(sight(sim)==sight(dry),"scouts gained different sight before reaching the hidden deposit");
            auto other=make_ai_commands(dry,player,dry_sequence);
            check(commands.size()==other.size(),"hidden salvage amount changed the number of AI orders");
            for (size_t j=0;j<other.size();++j) {
                check(serialize_command(commands[j])==serialize_command(other[j]),"hidden salvage amount changed AI order bytes");
                check(dry.submit(other[j]),"dry search command rejected");
            }
        } else if (!times.sight) {
            times.sight=sim.tick();
            check(sight(dry)==2,"empty patch was not rediscovered by the same scout");
        }
        for (const auto& c:commands) {
            if (searching) {
                check(c.order!=Order::Gather,"worker ordered to gather at unresolved hidden patch");
                if (c.order==Order::Move) ++search_moves;
                if (c.order==Order::TrainWorker) ++search_recruits;
            }
            if (c.order==Order::TrainWorker && sight(sim)==2) bought_at_patch=true;
            Command decoded;
            check(c.player==player && deserialize_command(serialize_command(c),decoded) &&
                sim.submit(c) && replay.submit(decoded),"finite salvage wire order rejected");
        }
        std::vector<int> cargo;
        for (const auto& u:sim.units()) cargo.push_back(u.cargo);
        const int old_bank=sim.salvage(player),old_reserve=patch.remaining;
        sim.step(); replay.step(); if (searching) dry.step();
        check(sim.state_hash()==replay.state_hash(),"finite salvage replay diverged");
        for (const auto& u:sim.units()) {
            if (u.player!=player || u.hp<=0 || u.kind!=UnitKind::Worker || u.resource_id!=patch.id) continue;
            if (u.id>cargo.size() || u.cargo_kind!=0) continue;
            const int previous=cargo[u.id-1];
            if (!times.mine && patch.remaining<old_reserve && u.cargo==previous+1) {
                times.mine=sim.tick();
                first_miner=u.id;
            }
            if (u.id==first_miner && previous>0 && !u.cargo && sim.salvage(player)==old_bank+previous)
                times.delivery=sim.tick();
        }
    }
    check(times.sight && times.mine>times.sight && times.delivery>times.mine,"last-50 economy never scouted, mined and returned fresh salvage");
    check(search_recruits==(survivor?0:1),"AI spent the last 50 on the wrong number of speculative workers");
    check(search_moves==1,"scout search requires a single uninterrupted approach order");
    check(!survivor || bought_at_patch,"surviving scout failed to use saved funds after sighting fresh salvage");
    for (int i=0;i<200;++i) {
        for (const auto& c:make_ai_commands(dry,player,dry_sequence)) {
            check(c.order!=Order::Gather && c.order!=Order::TrainWorker && c.order!=Order::Move,
                "AI kept searching or hiring after discovering the alternate was empty");
            check(dry.submit(c),"dry scout command rejected");
        }
        dry.step();
    }
    check(dry.salvage(player)==(survivor?kWorkerCost:0) && !dry.population_reserved(player),
        "discovering a dry patch changed the remaining scout budget");
    std::cout<<"finite search player="<<int(player)<<" survivor="<<survivor<<" remembered="<<(prior==PriorSight::Stocked)
             <<" sight="<<times.sight<<" mined="<<times.mine<<" delivered="<<times.delivery<<" hash="<<sim.state_hash()<<'\n';
    return times;
}

void exhausted_salvage(uint8_t player,bool foundry) {
    uint32_t sequence=0;
    auto sim=depleted_base(player,false,PriorSight::Empty,foundry,sequence);
    for (int i=0;i<200;++i) {
        for (const auto& c:make_ai_commands(sim,player,sequence)) {
            check(c.order!=Order::TrainWorker && c.order!=Order::Gather,"confirmed dry map consumed worker budget");
            check(sim.submit(c),"dry map command rejected");
        }
        sim.step();
    }
    const auto army=std::count_if(sim.units().begin(),sim.units().end(),[&](const Unit& u){
        return u.player==player && u.hp>0 && u.kind==UnitKind::Strider;
    });
    check(army==(foundry?1:0) && sim.salvage(player)==(foundry?0:kWorkerCost) && !sim.population_reserved(player),
        "confirmed exhaustion did not preserve cash or produce the affordable military unit");
    std::cout<<"finite exhausted player="<<int(player)<<" foundry="<<foundry<<" striders="<<army
             <<" bank="<<sim.salvage(player)<<" tick="<<sim.tick()<<" hash="<<sim.state_hash()<<'\n';
}

void visible_remnant_spending(uint8_t player,int survivors,int remnant,bool hire,PriorSight prior=PriorSight::Unseen) {
    uint32_t sequence=0;
    auto sim=depleted_base(player,survivors,prior,true,sequence,remnant);
    const auto& anchor=sim.structures()[player];
    const auto& foundry=sim.structures().back();
    for (const auto& c:make_ai_commands(sim,player,sequence)) check(sim.submit(c),"remnant AI command rejected");
    sim.step();
    check(sim.salvage(player)==0 && anchor.production_queue==uint32_t(hire) && foundry.production_queue==uint32_t(!hire),
        "AI spent its last worker cost without accounting for visible salvage and existing miners");
    if (survivors) return;
    int workers_bought=hire?1:0;
    bool earned=false,fielded_strider=false;
    // If this is the last known patch, a worker must be able to pay for
    // the military unit we could have fielded immediately with our last 50.
    for (int i=0;i<3000;++i) {
        for (const auto& c:make_ai_commands(sim,player,sequence)) {
            if (c.order==Order::TrainWorker) ++workers_bought;
            check(sim.submit(c),"final-remnant follow-up order rejected");
        }
        sim.step();
        earned=earned || sim.salvage(player)>0;
        fielded_strider=sim.units().back().player==player && sim.units().back().kind==UnitKind::Strider;
        if ((prior==PriorSight::Empty && fielded_strider) || (prior==PriorSight::Unseen && earned)) break;
    }
    check(workers_bought==(hire?1:0),"extra worker bought with only a final remnant left");
    if (prior==PriorSight::Empty) {
        check(fielded_strider && sim.salvage(player)==0 && !sim.population_reserved(player) &&
            sim.deposits()[player].remaining==(hire?0:remnant) && earned==hire,
            "last recoverable income did not field a Strider or preserve the unaffordable remnant");
    } else {
        check(earned && sim.units().back().kind==UnitKind::Worker && sim.salvage(player)==std::min(remnant,kCargoCapacity) &&
            sim.deposits()[player].remaining==remnant-sim.salvage(player),"scout recruit did not bank the visible remnant");
    }
    std::cout<<"finite final remnant player="<<int(player)<<" remote_dry_known="<<(prior==PriorSight::Empty)
             <<" stock="<<remnant<<" bought_workers="<<workers_bought<<" fielded_strider="<<fielded_strider
             <<" reserve_left="<<sim.deposits()[player].remaining<<" bank="<<sim.salvage(player)<<" tick="<<sim.tick()<<'\n';
}

void paid_army_scout(uint8_t player) {
    uint32_t sequence=0;
    auto sim=depleted_base(player,0,PriorSight::Unseen,true,sequence,0,true);
    const auto& remote=sim.deposits()[1-player];
    check(sim.structures().back().production_queue==1,"army scout should already be paid");
    // Occupy every exit in a copy. The finished but trapped Strider cannot
    // supply sight, so the anchor must use the last 50 to recruit a scout.
    auto blocked=sim;
    auto& blockers=const_cast<std::vector<Unit>&>(blocked.units());
    const auto& foundry=blocked.structures().back();
    for (int dx=-1;dx<=1;++dx) for (int dz=-1;dz<=1;++dz) if (dx || dz) {
        Unit u;
        u.id=static_cast<uint32_t>(blockers.size()+1); u.player=1-player; u.kind=UnitKind::Worker; u.order=Order::Hold;
        u.x=u.goal_x=u.next_x=foundry.x+dx*(kScale+64+32);
        u.z=u.goal_z=u.next_z=foundry.z+dz*(kScale+64+32);
        blockers.push_back(u);
    }
    step(blocked,kProductionTicks);
    check(foundry.spawn_blocked && foundry.production_queue==1,"reserved army did not reach the blocked exit");
    auto blocked_sequence=sequence;
    for (const auto& c:make_ai_commands(blocked,player,blocked_sequence)) check(blocked.submit(c),"blocked scout fallback rejected");
    blocked.step();
    check(foundry.spawn_blocked && blocked.salvage(player)==0 && blocked.structures()[player].production_queue==1,
        "trapped army prevented the last funded worker from scouting");
    bool fielded=false,recruited=false;
    for (int i=0;i<3000 && !recruited;++i) {
        const bool saw_patch=sim.visibility(player,remote.x/kScale,remote.z/kScale)==2;
        for (const auto& c:make_ai_commands(sim,player,sequence)) {
            if (c.order==Order::TrainWorker) {
                check(saw_patch,"AI spent its reserve before the army found salvage");
                recruited=true;
            }
            check(sim.submit(c),"army scout command rejected");
        }
        sim.step();
        if (!recruited) check(sim.salvage(player)==kWorkerCost,"AI used the worker's reserve while its army scouted");
        fielded=fielded || sim.units().back().kind==UnitKind::Strider;
    }
    check(fielded && recruited && sim.salvage(player)==0 && sim.structures()[player].production_queue==1,
        "paid army did not scout salvage and leave enough for the first recruit");
    std::cout<<"finite paid army scout player="<<int(player)<<" recruit_tick="<<sim.tick()<<" hash="<<sim.state_hash()<<'\n';
}

void gather_through_fog(uint8_t player) {
    Sim sim(42,3,Map::Economy);
    const uint32_t miner=3u*player+1,scout=miner+1;
    const auto& patch=sim.deposits()[1-player];
    const auto sight=[&](const Sim& s){ return s.visibility(player,patch.x/kScale,patch.z/kScale); };
    const auto exhaust=[&](Sim& s){ const_cast<std::vector<Deposit>&>(s.deposits())[1-player].remaining=0; };
    const auto gather_at_patch=[&](Sim& s,uint32_t seq){
        check(s.submit({s.tick(),seq,player,Order::Gather,{miner},patch.x,patch.z}),"remote gather rejected at submission");
        s.step();
    };
    const auto check_blind_gather=[&](const Sim& source,uint32_t seq) {
        auto stocked=source,dry=source;
        exhaust(dry);
        for (auto* world:{&stocked,&dry}) {
            gather_at_patch(*world,seq);
            check(world->command_result(player)==CommandResult::Accepted && world->units()[miner-1].order==Order::Gather,
                "blind Gather result disclosed hidden depletion");
        }
    };
    const auto check_until_visible=[&](Sim& stocked,Sim& dry) {
        const uint32_t start=stocked.tick();
        while (sight(stocked)<2 && stocked.tick()-start<3000) {
            const auto& a=stocked.units()[miner-1];
            const auto& b=dry.units()[miner-1];
            check(sight(dry)==sight(stocked) && a.order==Order::Gather && b.order==a.order &&
                a.x==b.x && a.z==b.z && a.goal_x==b.goal_x && a.goal_z==b.goal_z &&
                a.cargo==b.cargo && a.returning==b.returning && stocked.salvage(player)==dry.salvage(player),
                "active Gather disclosed depletion while its target was dark");
            stocked.step(); dry.step();
        }
        check(stocked.tick()>start && sight(stocked)==2 && sight(dry)==2,"assigned miner did not regain team sight");
        stocked.step(); dry.step();
        check(dry.units()[miner-1].order==Order::Stop && stocked.units()[miner-1].order==Order::Gather,
            "assigned miner failed to react to observed exhaustion");
    };
    check(sight(sim)==0,"remote patch unexpectedly visible on opening");
    check_blind_gather(sim,1);
    const int side=player==0?-1:1;
    check(sim.submit({0,1,player,Order::Move,{scout},patch.x+side*2*kScale,patch.z}),"scout order rejected");
    for (int i=0;i<2000 && sight(sim)!=2;++i) sim.step();
    check(sight(sim)==2,"preliminary scout failed to reveal the remote deposit");
    gather_at_patch(sim,2);
    const_cast<std::vector<Unit>&>(sim.units())[scout-1].hp=0;
    sim.step();
    check(sight(sim)==1,"scout loss did not darken the remote deposit");
    check_blind_gather(sim,3);
    auto dry=sim;
    exhaust(dry);
    check_until_visible(sim,dry);
    gather_at_patch(dry,3);
    check(dry.command_result(player)==CommandResult::InvalidTarget,"observably empty Gather target accepted");

    for (int i=0;i<2000 && !(sim.units()[miner-1].returning && sight(sim)==1);++i) sim.step();
    check(sight(sim)==1 && sim.units()[miner-1].cargo==kCargoCapacity,"returning miner failed to leave sight with a full load");
    dry=sim;
    exhaust(dry);
    const int old_bank=sim.salvage(player);
    check_until_visible(sim,dry);
    check(sim.salvage(player)==old_bank+kCargoCapacity && dry.salvage(player)==sim.salvage(player),
        "hidden depletion interrupted cargo delivery or the subsequent trip");
    std::cout<<"fog Gather player="<<int(player)<<" second_sighting="<<sim.tick()<<" delivered="<<kCargoCapacity
             <<" stocked_hash="<<sim.state_hash()<<" dry_hash="<<dry.state_hash()<<'\n';
}
}
int main() {
    try {
        validation(); interruption_and_depletion(); construction_and_routes(); occupied_service_goal(); competing_purchases(); replay_and_apply_time();
        worker_replacement(0,0,kWorkerCost);
        worker_replacement(1,1,3*kWorkerCost);
        worker_replacement(1,0,kWorkerCost-kCargoCapacity);
        worker_replacement(0,0,kWorkerCost-kCargoCapacity);
        for (auto sight:{PriorSight::Unseen,PriorSight::Stocked}) for (bool survivor:{true,false}) {
            const auto first=finite_salvage_search(0,survivor,sight);
            check(first==finite_salvage_search(1,survivor,sight),"rotated AI scouting, extraction or delivery timing differed");
        }
        for (uint8_t player=0;player<2;++player) {
            exhausted_salvage(player,false);
            exhausted_salvage(player,true);
            gather_through_fog(player);
            visible_remnant_spending(player,0,1,true);
            visible_remnant_spending(player,0,kWorkerCost,true);
            visible_remnant_spending(player,0,1,false,PriorSight::Empty);
            visible_remnant_spending(player,0,kWorkerCost,true,PriorSight::Empty);
            visible_remnant_spending(player,1,49,false);
            visible_remnant_spending(player,1,50,true);
            visible_remnant_spending(player,2,99,false);
            visible_remnant_spending(player,2,100,true);
            paid_army_scout(player);
        }
        std::cout<<"economy tests passed\n";
    }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
