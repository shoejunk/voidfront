#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vf::nav {

struct Point {
    std::int32_t x{}, z{};
    bool operator==(const Point&) const = default;
};

struct Rect {
    std::int32_t min_x{}, min_z{}, max_x{}, max_z{};
};

// Exact integer ceiling, including UINT64_MAX (whose result is 2^32).
std::uint64_t ceil_sqrt(std::uint64_t value);

// The closed segment may touch the rectangle boundary, but not its open
// interior. All coordinates must have absolute value <=65536 and the rectangle
// must have positive width/height; unsupported inputs throw invalid_argument.
// This larger limit includes World obstacle expansion and swept unit bounds.
bool segment_clear(Point a, Point b, Rect forbidden);

// Bounded static visibility navigation for the current small skirmish map.
// Input coordinates have absolute value <=32768, clearance is in [0,32768],
// and at most 64 proper rectangles are accepted. Rectangles may cross the map
// bounds. Clearance is an axis-aligned square half-extent: bounds are shrunk,
// obstacles expanded, and boundary tangency is permitted. Unsupported input or
// more than 1024 exposed graph vertices throws invalid_argument.
//
// Construction caches all visible static edges in O(V^2 * R) time/O(V^2)
// memory. Each query costs O(V * R + V^2), with no runtime clocks or scheduling.
// This is not the scalable dynamic/crowd pathfinder required by the final game.
class World {
public:
    World(Rect bounds, std::vector<Rect> obstacles, std::int32_t clearance);
    bool valid(Point p) const;
    bool clear(Point a, Point b) const;

    // Excludes start and includes the exact goal. Empty means invalid,
    // unreachable, or start==goal. Edge cost is ceil(Euclidean length) in
    // integer coordinate units; stable lexicographic vertices break ties.
    std::vector<Point> route(Point start, Point goal) const;

private:
    struct Edge {
        std::size_t node;
        std::uint64_t cost;
    };
    Rect bounds_;
    std::vector<Rect> obstacles_;
    std::vector<Point> vertices_;
    std::vector<std::vector<Edge>> edges_;
};

} // namespace vf::nav
