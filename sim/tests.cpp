#include "voidfront_sim.hpp"
#include <cstdlib>
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
    try { wire_validation(); command_validation(); routing_and_stop(); replay_combat_and_crowds(); late_receipt_after_death(); }
    catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return EXIT_FAILURE; }
    std::cout << "PASS: protocol, ownership, ordering, terrain, stop, crowds, AI combat, replay\n";
}
