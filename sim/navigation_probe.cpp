#include "navigation.hpp"
#include <cstdint>
#include <iostream>
#include <vector>
#include <stdexcept>

// One whitespace-delimited fixture per JSON output line. No floating point is
// used here: Euclidean reference measurements belong in the offline verifier.
int main() {
    using namespace vf::nav;
    Rect bounds{};
    while (std::cin >> bounds.min_x) {
        int32_t clearance = 0;
        int count = 0;
        if (!(std::cin >> bounds.min_z >> bounds.max_x >> bounds.max_z >> clearance >> count)
            || count < 0 || count > 256) return 2;
        std::vector<Rect> obstacles(static_cast<size_t>(count));
        for (auto& r : obstacles)
            if (!(std::cin >> r.min_x >> r.min_z >> r.max_x >> r.max_z)) return 2;
        Point start{}, goal{};
        if (!(std::cin >> start.x >> start.z >> goal.x >> goal.z)) return 2;
        try {
        const World world(bounds, obstacles, clearance);
        const auto path = world.route(start, goal);
        std::cout << "{\"valid\":" << "true"
            << ",\"start_clear\":" << (world.valid(start) ? "true" : "false")
            << ",\"goal_clear\":" << (world.valid(goal) ? "true" : "false")
            << ",\"direct_clear\":" << (world.clear(start, goal) ? "true" : "false")
            << ",\"route\":[";
        for (size_t i = 0; i < path.size(); ++i) {
            if (i) std::cout << ',';
            std::cout << '[' << path[i].x << ',' << path[i].z << ']';
        }
        std::cout << "]}\n";
        } catch (const std::invalid_argument&) {
            std::cout << "{\"valid\":false,\"start_clear\":false,\"goal_clear\":false,\"direct_clear\":false,\"route\":[]}\n";
        }
    }
    return std::cin.eof() ? 0 : 2;
}
