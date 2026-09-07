#include "voidfront_sim.hpp"
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace vf;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
Command move(const Sim& s, uint32_t sequence, int x, int z) {
    Command c{s.tick(), sequence, 0, Order::Move, {1}, x * kScale + 128, z * kScale + 128};
    return c;
}
void wire_validation() {
    Sim s;
    auto c = move(s, 1, 12, 10);
    const auto bytes = serialize_command(c);
    Command decoded;
    check(deserialize_command(bytes, decoded), "wire roundtrip rejected");
    check(serialize_command(decoded) == bytes, "wire bytes changed");
    for (size_t n = 0; n < bytes.size(); ++n)
        check(!deserialize_command(std::span(bytes).first(n), decoded), "truncated wire accepted");
    auto bad = bytes; bad[3] = 2;
    check(!deserialize_command(bad, decoded), "incompatible version accepted");
    bad = bytes; bad.push_back(0);
    check(!deserialize_command(bad, decoded), "trailing bytes accepted");
    bad = bytes; bad[13] = 255;
    check(!deserialize_command(bad, decoded), "invalid order accepted");
    bad = bytes; bad[22] = 255; bad[23] = 255;
    check(!deserialize_command(bad, decoded), "oversized unit list accepted");
    c.units = {1,1}; check(serialize_command(c).empty(), "duplicate wire IDs accepted");
}
void command_validation() {
    Sim a, b;
    auto c = move(a, 1, 12, 10);
    c.player = 1;
    check(!a.submit(c), "foreign ownership accepted");
    check(a.hash() == b.hash(), "rejected command mutated sim");
    c.player = 0; c.units = {0}; check(!a.submit(c), "zero ID accepted");
    c.units = {1}; c.x = -1; check(!a.submit(c), "negative coordinate accepted");
    c = move(a, 1, 12, 10);
    check(a.submit(c), "valid command rejected");
    check(!a.submit(c), "duplicate command accepted");
    a.step(); check(!a.submit(c), "past command accepted");
    c = move(a, 2, 12, 10); c.tick += 1201;
    check(!a.submit(c), "unbounded future command accepted");
    Sim ordered, reordered;
    auto first = move(ordered, 1, 12, 10), second = move(ordered, 2, 10, 12);
    first.tick = 2; second.tick = 3;
    check(ordered.submit(first) && ordered.submit(second), "ordered commands rejected");
    check(reordered.submit(second) && reordered.submit(first), "reordered commands rejected");
    check(ordered.hash() == reordered.hash(), "pending arrival order changes hash");
    for (int i = 0; i < 100; ++i) { ordered.step(); reordered.step(); check(ordered.hash() == reordered.hash(), "reordered execution differs"); }
}
void routing_and_stop() {
    Sim s(1,1);
    auto c = move(s, 1, 24, 4);
    check(s.submit(c), "route command rejected");
    for (int i = 0; i < 500; ++i) {
        s.step(); const auto& u = s.units()[0];
        check(!s.blocked(u.x / kScale, u.z / kScale), "unit entered terrain");
    }
    check(s.units()[0].x == c.x && s.units()[0].z == c.z, "route around ridge did not arrive");
    c = move(s, 2, 5, 5); check(s.submit(c), "return move rejected");
    s.step(); s.step();
    c = move(s, 3, 0, 0); c.order = Order::Stop; check(s.submit(c), "stop rejected");
    const auto x = s.units()[0].x, z = s.units()[0].z;
    s.step();
    check(s.units()[0].x == x && s.units()[0].z == z, "stop moved during authoritative response tick");
    for (int i = 0; i < 30; ++i) s.step();
    check(s.units()[0].x == x && s.units()[0].z == z, "stopped unit drifted");
    c = move(s, 4, 15, 5); check(s.submit(c), "blocked goal command rejected");
    for (int i = 0; i < 400; ++i) s.step();
    check(!s.blocked(s.units()[0].x/kScale,s.units()[0].z/kScale), "blocked goal enters terrain");
}
void any_angle_precision() {
    const nav::World world({256,256,31*256,23*256},
        {{15*256,3*256,17*256,9*256},{15*256,16*256,17*256,21*256}},64);
    for (const auto goal : {nav::Point{3101,1109},nav::Point{3151,4473},nav::Point{6221,1173}}) {
        Sim s(1,1);
        Command c{0,1,0,Order::Move,{1},goal.x,goal.z};
        check(s.submit(c), "precise movement rejected");
        bool oblique=false;
        for (int tick=0;tick<600;++tick) {
            const nav::Point before{s.units()[0].x,s.units()[0].z};
            s.step();
            const nav::Point after{s.units()[0].x,s.units()[0].z};
            const int64_t dx=after.x-before.x,dz=after.z-before.z;
            check(world.clear(before,after), "movement swept terrain clearance");
            check(dx*dx+dz*dz<=32*32, "diagonal movement exceeded speed");
            if (dx && dz && std::abs(dx)!=std::abs(dz)) oblique=true;
        }
        check(oblique,"authoritative movement remained grid aligned");
        check(s.units()[0].x==goal.x && s.units()[0].z==goal.z,"off-center destination not reached exactly");
        check(s.units()[0].order==Order::Stop,"arrived move did not stop");
    }
    // Independent critic's valid tangent route: one-sided truncation used to
    // stick at (4407,2390) because the next integer sample penetrated the ridge.
    Sim tangent(1,1);
    for (const auto goal : {nav::Point{3968,3920},nav::Point{4570,1835}}) {
        check(tangent.submit(Command{tangent.tick(),tangent.tick()+1,0,Order::Move,{1},goal.x,goal.z}),"tangent order rejected");
        for(int i=0;i<400;++i) {
            const nav::Point before{tangent.units()[0].x,tangent.units()[0].z};
            tangent.step();
            check(world.clear(before,{tangent.units()[0].x,tangent.units()[0].z}),"tangent swept clearance violated");
        }
        check(tangent.units()[0].x==goal.x && tangent.units()[0].z==goal.z,"tangent lattice rounding permanently stalled");
    }
    // Retarget in mid-segment; the obsolete waypoint must not retain authority.
    Sim s(1,1);
    check(s.submit(Command{0,1,0,Order::Move,{1},3101,1109}),"first heading rejected");
    for(int i=0;i<9;++i) s.step();
    const auto before=s.units()[0];
    check(s.submit(Command{s.tick(),2,0,Order::Move,{1},450,4001}),"retarget rejected");
    s.step();
    check(s.units()[0].x<before.x && s.units()[0].z>before.z,"retarget followed stale waypoint");
}
void attack_move_resumes_after_combat() {
    Sim s(1,1);
    check(s.submit(Command{0,1,0,Order::AttackMove,{1},7552,2432}),"resume attack-move rejected");
    for(int i=0;i<250 && s.units()[0].x<6752;++i) s.step();
    check(s.units()[0].x==6752,"resume fixture did not reach engagement staging point");
    // Enemy steps into range while executing Move, giving the attacker the
    // first shot so it survives and can exercise route resumption after combat.
    check(s.submit(Command{s.tick(),1,1,Order::Move,{2},7488,2432}),"resume enemy staging rejected");
    for(int i=0;i<350;++i) s.step();
    check(s.units()[1].hp==0 && s.units()[0].hp>0,"resume fixture did not leave attacker alive");
    check(s.units()[0].x==7552 && s.units()[0].z==2432,"attack-move lost route after combat pause");
}
void replay_combat_and_crowds() {
    Sim a(42), b(42);
    std::array<uint32_t,2> seq{};
    for (int tick = 0; tick < 1600; ++tick) {
        for (uint8_t p=0; p<2; ++p) for (auto c : make_ai_commands(a,p,seq[p])) {
            Command decoded; check(deserialize_command(serialize_command(c),decoded), "replay decode failed");
            check(a.submit(c) && b.submit(decoded), "AI replay command rejected");
        }
        a.step(); b.step(); check(a.hash()==b.hash(), "replay diverged");
        for (const auto& u:a.units()) if (u.hp>0) {
            check(!a.blocked(u.x/kScale,u.z/kScale), "crowd entered terrain");
            for(const auto& v:a.units()) if(v.hp>0 && v.id>u.id) {
                const int64_t dx=u.x-v.x,dz=u.z-v.z;
                check(dx*dx+dz*dz >= int64_t(128)*128, "crowd overlap");
            }
        }
    }
    check(a.winner()!=-1, "skirmish AI never concluded");
    Sim reset(42); check(reset.tick()==0 && reset.hash()!=a.hash(), "reset construction failed");
    Sim different(43); check(reset.hash()!=different.hash(), "seed not in deterministic state");
    std::cout << "replay_hash=" << a.hash() << " winner=" << a.winner() << '\n';
}
void late_receipt_after_death() {
    Sim early(42), late(42);
    auto future = move(early, 10000, 10, 10);
    future.tick = 1000;
    check(early.submit(future), "early future command rejected");
    std::array<uint32_t,2> seq{};
    for (int tick = 0; tick < 950; ++tick) {
        for (uint8_t p = 0; p < 2; ++p) for (const auto& c : make_ai_commands(early,p,seq[p]))
            check(early.submit(c) && late.submit(c), "receipt regression AI rejected");
        early.step(); late.step();
    }
    check(late.units()[0].hp == 0, "receipt regression did not kill command subject");
    check(late.submit(future), "future command admission depends on receipt-time health");
    check(early.hash() == late.hash(), "late receipt changed pending state");
    for (int tick = 950; tick <= 1000; ++tick) {
        early.step(); late.step(); check(early.hash() == late.hash(), "dead unit command execution diverged");
    }
}
}
int main() {
    try { wire_validation(); command_validation(); routing_and_stop(); any_angle_precision(); attack_move_resumes_after_combat(); replay_combat_and_crowds(); late_receipt_after_death(); }
    catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return EXIT_FAILURE; }
    std::cout << "PASS: protocol, ownership, ordering, terrain, stop, crowds, AI combat, replay\n";
}
