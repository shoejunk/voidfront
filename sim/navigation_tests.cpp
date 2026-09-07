#include "navigation.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace vf::nav;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void static_routes() {
    const World open({0, 0, 4096, 4096}, {}, 64);
    check(open.route({129, 137}, {3821, 2673}) == std::vector<Point>{{3821, 2673}},
        "unobstructed oblique route is not direct");
    check(open.route({129, 137}, {130, 139}) == std::vector<Point>{{130, 139}},
        "tiny off-center route snapped");
    check(open.route({129, 137}, {129, 137}).empty(), "same-point route nonempty");
    check(open.valid({64, 64}) && !open.valid({63, 64}), "bounds clearance incorrect");
    const World box({0, 0, 4096, 4096}, {{1500, 1000, 2500, 3000}}, 64);
    check(box.valid({1436, 1500}) && !box.valid({1437, 1500}), "box clearance incorrect");
    check(box.clear({1436, 64}, {1436, 4032}), "tangency rejected");
    check(!box.clear({1437, 64}, {1437, 4032}), "one-unit penetration accepted");
    check(!box.clear({512, 2048}, {3584, 2048}), "obstructed segment accepted");
    const auto path = box.route({512, 2048}, {3584, 2048});
    check(!path.empty() && path.back() == Point{3584, 2048}, "box detour absent");
    auto prev = Point{512, 2048};
    for (auto p : path) { check(box.clear(prev, p), "route segment obstructed"); prev = p; }
    for (int repeat = 0; repeat < 10; ++repeat)
        check(box.route({512, 2048}, {3584, 2048}) == path, "route tie nondeterministic");
    const World blocked({0, 0, 4096, 4096}, {{1900, 0, 2100, 4096}}, 64);
    check(blocked.route({512, 2048}, {3584, 2048}).empty(), "disconnected route found");
    const World pass({0, 0, 4096, 4096}, {{1900, 0, 2100, 1984}, {1900, 2112, 2100, 4096}}, 64);
    check(pass.clear({512, 2048}, {3584, 2048}), "exact-clearance passage rejected");
    const World narrow({0, 0, 4096, 4096}, {{1900, 0, 2100, 1985}, {1900, 2112, 2100, 4096}}, 64);
    check(narrow.route({512, 2048}, {3584, 2048}).empty(), "too-narrow passage accepted");
}
void malformed_worlds() {
    auto rejects = [](Rect bounds, std::vector<Rect> obstacles, int clearance) {
        try { const World world(bounds, obstacles, clearance); }
        catch (const std::invalid_argument&) { return true; }
        return false;
    };
    check(rejects({0, 0, 0, 100}, {}, 0), "zero-width bounds accepted");
    check(rejects({0, 0, 32769, 100}, {}, 0), "coordinate limit ignored");
    check(rejects({-32769, 0, 100, 100}, {}, 0), "negative coordinate limit ignored");
    check(rejects({0, 0, 100, 100}, {}, -1), "negative clearance accepted");
    check(rejects({0, 0, 100, 100}, {}, 51), "inverted shrunken bounds accepted");
    check(rejects({0, 0, 100, 100}, {{20, 20, 20, 30}}, 0), "empty obstacle accepted");
    check(rejects({0, 0, 100, 100}, std::vector<Rect>(65, {20, 20, 30, 30}), 0),
        "obstacle capacity ignored");
    check(ceil_sqrt(0) == 0 && ceil_sqrt(1) == 1 && ceil_sqrt(2) == 2
        && ceil_sqrt(UINT64_MAX) == (uint64_t{1} << 32), "ceil sqrt edge case");
}
}
int main() {
    try { static_routes(); malformed_worlds(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Navigation static geometry and malformed-input tests passed\n";
}
