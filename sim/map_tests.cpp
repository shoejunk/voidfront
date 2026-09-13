#include "voidfront_sim.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <tuple>

namespace {
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
void assignment(vf::Map map,uint32_t count,int x,int z) {
    vf::Sim sim(42,count,map);
    vf::Command command{0,1,0,vf::Order::Move,{},x,z};
    for (const auto& u:sim.units()) if (u.player==0) command.units.push_back(u.id);
    // Independent exhaustive distance/row/column sort, not the production rings.
    std::vector<std::tuple<int,int,int>> cells;
    for (int row=0;row<sim.height();++row) for (int col=0;col<sim.width();++col)
        if (!sim.blocked(col,row)) cells.emplace_back(std::abs(col-x/256)+std::abs(row-z/256),row,col);
    std::sort(cells.begin(),cells.end());
    check(sim.submit(command),"group command rejected"); sim.step();
    for (uint32_t i=0;i<count;++i) {
        const auto [distance,row,col]=cells[i];
        check(sim.units()[i].goal_x==col*256+128 && sim.units()[i].goal_z==row*256+128,
            "group assignment differs from exhaustive row-major reference");
    }
}
void maps() {
    for (const auto map:{vf::Map::Foundry,vf::Map::Scale128}) {
        vf::Sim sim(42,250,map);
        check(sim.width()==(map==vf::Map::Foundry?32:128) && sim.height()==(map==vf::Map::Foundry?24:128),"map dimensions wrong");
        vf::nav::World world({256,256,(sim.width()-1)*256,(sim.height()-1)*256},vf::map_terrain(map),64);
        for (const auto& u:sim.units()) {
            check(world.valid({u.x,u.z}),"spawn lacks terrain clearance");
            for (const auto& v:sim.units()) if (u.id<v.id)
                check(std::abs(u.x-v.x)>=128 || std::abs(u.z-v.z)>=128,"spawn overlap");
        }
        for (const uint32_t count:{2u,17u,100u,250u})
            for (const auto point:{vf::nav::Point{0,0},{(sim.width()-1)*256,0},
                {0,(sim.height()-1)*256},{(sim.width()-1)*256,(sim.height()-1)*256},
                {sim.width()*128,sim.height()*128},{(sim.width()/2-1)*256,sim.height()*64}})
                assignment(map,count,point.x,point.z);
        vf::Command outside{0,1,0,vf::Order::Move,{1},sim.width()*256,0};
        const auto before=sim.hash();
        check(!sim.submit(outside) && sim.hash()==before,"out-of-map command accepted or mutated state");
        outside.x=0; outside.z=sim.height()*256;
        check(!sim.submit(outside) && sim.hash()==before,"out-of-map z accepted or mutated state");
    }
    bool rejected=false;
    try { vf::Sim invalid(42,6,static_cast<vf::Map>(2)); } catch (const std::invalid_argument&) { rejected=true; }
    check(rejected,"invalid map accepted");
    vf::Sim small(42,1),large(42,1,vf::Map::Scale128);
    check(small.state_hash()!=large.state_hash(),"map not distinguished in state hash");
    vf::Command far{0,1,0,vf::Order::Move,{1},110*256+77,63*256+13},decoded;
    check(vf::deserialize_command(vf::serialize_command(far),decoded),"large coordinate wire roundtrip rejected");
    check(!small.submit(decoded) && large.submit(decoded),"map-specific command admission wrong");
    vf::nav::World world({256,256,127*256,127*256},vf::map_terrain(vf::Map::Scale128),64);
    for (int tick=0;tick<1400;++tick) {
        const vf::nav::Point before{large.units()[0].x,large.units()[0].z}; large.step();
        check(world.clear(before,{large.units()[0].x,large.units()[0].z}),"large route entered terrain");
    }
    check(large.units()[0].x==far.x && large.units()[0].z==far.z,"large-map exact destination not reached");
    far.x=32768; check(vf::serialize_command(far).empty(),"wire coordinate limit not enforced");
    for (const auto point:{vf::nav::Point{32767,0},{0,32767},{32767,32767}}) {
        vf::Sim boundary(42,1,vf::Map::Scale128);
        vf::Command edge{0,1,0,vf::Order::Move,{1},point.x,point.z},copy;
        check(vf::deserialize_command(vf::serialize_command(edge),copy) && boundary.submit(copy),"valid maximum coordinate rejected");
    }
    for (const auto point:{vf::nav::Point{-1,0},{0,-1},{32768,0},{0,32768}}) {
        vf::Command bad{0,1,0,vf::Order::Move,{1},point.x,point.z};
        check(vf::serialize_command(bad).empty(),"wire axis limit accepted");
    }
    for (const auto point:{vf::nav::Point{8191,6143},{0,0}}) {
        vf::Sim boundary(42,1);
        check(boundary.submit({0,1,0,vf::Order::Move,{1},point.x,point.z}),"valid Foundry boundary rejected");
    }
}
void dead_selection() {
    vf::Sim sim(42); std::array<uint32_t,2> sequences{};
    const vf::Unit* casualty=nullptr;
    for (int tick=0;tick<1500 && !casualty;++tick) {
        for (uint8_t p=0;p<2;++p) for (const auto& command:vf::make_ai_commands(sim,p,sequences[p]))
            check(sim.submit(command),"casualty fixture command rejected");
        sim.step();
        for (const auto& unit:sim.units()) if (unit.hp==0) { casualty=&unit; break; }
    }
    check(casualty!=nullptr,"no dead selection subject");
    vf::Command command{sim.tick(),++sequences[casualty->player],casualty->player,vf::Order::Move,{},128,128};
    for (const auto& u:sim.units()) if (u.player==command.player) command.units.push_back(u.id);
    std::vector<std::tuple<int,int,int>> cells;
    for (int z=0;z<sim.height();++z) for (int x=0;x<sim.width();++x)
        if (!sim.blocked(x,z)) cells.emplace_back(x+z,z,x);
    std::sort(cells.begin(),cells.end());
    check(sim.submit(command),"mixed live/dead selection rejected"); sim.step();
    size_t slot=0;
    for (const auto& u:sim.units()) if (u.player==command.player && u.hp>0) {
        const auto [distance,z,x]=cells[slot++];
        check(u.goal_x==x*256+128 && u.goal_z==z*256+128,"dead unit consumed assignment slot");
    }
    check(slot>0,"mixed selection had no live subjects");
}
}
int main() {
    try { maps(); dead_selection(); } catch (const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
    std::cout<<"PASS: map bounds, 500 spawns, large-map precise route, wire admission, exhaustive group assignment\n";
}
