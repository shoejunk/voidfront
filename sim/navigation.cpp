#include "navigation.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace vf::nav {
namespace {

constexpr std::int32_t input_limit = 32768;
constexpr std::int32_t expanded_limit = input_limit * 2;
constexpr std::size_t max_rectangles = 64;
constexpr std::size_t max_vertices = 1024;

bool bounded(std::int32_t value, std::int32_t limit) {
    return value >= -limit && value <= limit;
}

bool bounded(Point point, std::int32_t limit) {
    return bounded(point.x, limit) && bounded(point.z, limit);
}

bool proper(Rect rect, std::int32_t limit) {
    return bounded(Point{rect.min_x, rect.min_z}, limit) &&
        bounded(Point{rect.max_x, rect.max_z}, limit) &&
        rect.min_x < rect.max_x && rect.min_z < rect.max_z;
}

bool inside(Point point, Rect rect) {
    return point.x > rect.min_x && point.x < rect.max_x &&
        point.z > rect.min_z && point.z < rect.max_z;
}

// Denominators are positive. Expanded coordinates are bounded to +/-65536;
// differences are <=131072, so these signed products are <=2^34, far below
// INT64_MAX. No floating-point tolerances or platform-specific wide integers.
struct Fraction {
    std::int64_t numerator;
    std::int64_t denominator;
};

bool less(Fraction a, Fraction b) {
    return a.numerator * b.denominator < b.numerator * a.denominator;
}

bool avoids_interior(Point a, Point b, Rect rect) {
    Fraction low{0, 1}, high{1, 1};
    const auto clip = [&low, &high](std::int32_t from, std::int32_t to,
                                  std::int32_t minimum, std::int32_t maximum) {
        const auto delta = static_cast<std::int64_t>(to) - from;
        if (delta == 0) return from > minimum && from < maximum;
        Fraction enter{static_cast<std::int64_t>(minimum) - from, delta};
        Fraction leave{static_cast<std::int64_t>(maximum) - from, delta};
        if (delta < 0) {
            enter = {static_cast<std::int64_t>(from) - maximum, -delta};
            leave = {static_cast<std::int64_t>(from) - minimum, -delta};
        }
        if (less(low, enter)) low = enter;
        if (less(leave, high)) high = leave;
        return less(low, high);
    };
    // Obstacle slab intervals are open. A nonempty interval after intersecting
    // [0,1] enters the interior; equality represents permitted boundary contact.
    return !(clip(a.x, b.x, rect.min_x, rect.max_x) &&
             clip(a.z, b.z, rect.min_z, rect.max_z));
}

std::uint64_t cost(Point a, Point b) {
    const auto x = static_cast<std::int64_t>(a.x) - b.x;
    const auto z = static_cast<std::int64_t>(a.z) - b.z;
    return ceil_sqrt(static_cast<std::uint64_t>(x * x + z * z));
}

} // namespace

std::uint64_t ceil_sqrt(std::uint64_t value) {
    if (value == 0) return 0;
    std::uint64_t low = 0, high = std::uint64_t{1} << 32;
    while (low + 1 < high) {
        const auto middle = low + (high - low) / 2;
        if (middle <= value / middle) low = middle;
        else high = middle;
    }
    return low * low == value ? low : low + 1;
}

bool segment_clear(Point a, Point b, Rect forbidden) {
    if (!bounded(a, expanded_limit) || !bounded(b, expanded_limit) ||
        !proper(forbidden, expanded_limit))
        throw std::invalid_argument("navigation segment exceeds supported geometry");
    return avoids_interior(a, b, forbidden);
}

World::World(Rect bounds, std::vector<Rect> obstacles, std::int32_t clearance) {
    if (!proper(bounds, input_limit) || clearance < 0 || clearance > input_limit ||
        obstacles.size() > max_rectangles)
        throw std::invalid_argument("navigation world exceeds supported geometry");
    for (const auto rect : obstacles) {
        if (!proper(rect, input_limit))
            throw std::invalid_argument("navigation obstacle exceeds supported geometry");
    }
    bounds_ = {bounds.min_x + clearance, bounds.min_z + clearance,
               bounds.max_x - clearance, bounds.max_z - clearance};
    if (bounds_.min_x > bounds_.max_x || bounds_.min_z > bounds_.max_z)
        throw std::invalid_argument("navigation clearance consumes map bounds");
    obstacles_.reserve(obstacles.size());
    for (const auto rect : obstacles)
        obstacles_.push_back({rect.min_x - clearance, rect.min_z - clearance,
                              rect.max_x + clearance, rect.max_z + clearance});

    // Rectangle corners alone do not represent intersections of overlapping
    // expanded obstacles or their intersections with the shrunken map boundary.
    // Enumerate vertical/horizontal edge intersections, then retain only exposed
    // points. Integer axis alignment makes every candidate exactly representable.
    auto boundaries = obstacles_;
    boundaries.push_back(bounds_);
    for (const auto vertical : boundaries) {
        for (const auto horizontal : boundaries) {
            for (const auto x : {vertical.min_x, vertical.max_x}) {
                for (const auto z : {horizontal.min_z, horizontal.max_z}) {
                    const Point candidate{x, z};
                    if (x >= horizontal.min_x && x <= horizontal.max_x &&
                        z >= vertical.min_z && z <= vertical.max_z && valid(candidate))
                        vertices_.push_back(candidate);
                }
            }
        }
    }
    std::sort(vertices_.begin(), vertices_.end(), [](Point a, Point b) {
        return a.x != b.x ? a.x < b.x : a.z < b.z;
    });
    vertices_.erase(std::unique(vertices_.begin(), vertices_.end()), vertices_.end());
    if (vertices_.size() > max_vertices)
        throw std::invalid_argument("navigation exposed vertex limit exceeded");
    edges_.resize(vertices_.size());
    for (std::size_t a = 0; a < vertices_.size(); ++a) {
        for (std::size_t b = a + 1; b < vertices_.size(); ++b) {
            if (clear(vertices_[a], vertices_[b])) {
                const auto weight = cost(vertices_[a], vertices_[b]);
                edges_[a].push_back({b, weight});
                edges_[b].push_back({a, weight});
            }
        }
    }
}

bool World::valid(Point p) const {
    if (p.x < bounds_.min_x || p.x > bounds_.max_x ||
        p.z < bounds_.min_z || p.z > bounds_.max_z) return false;
    for (const auto rect : obstacles_) if (inside(p, rect)) return false;
    return true;
}

bool World::clear(Point a, Point b) const {
    if (!valid(a) || !valid(b)) return false;
    // The shrunken bounds are convex, so valid endpoints imply the segment
    // remains in them. Full (unclipped) expanded obstacles supply collision.
    for (const auto rect : obstacles_) if (!avoids_interior(a, b, rect)) return false;
    return true;
}

std::vector<Point> World::route(Point start, Point goal) const {
    if (!valid(start) || !valid(goal) || start == goal) return {};
    if (clear(start, goal)) return {goal};
    const auto count = vertices_.size();
    const auto infinity = std::numeric_limits<std::uint64_t>::max();
    // Goal is an ephemeral final vertex; start connections initialize distances.
    // At most 1025 vertices and edge costs <100000 keep every finite distance
    // far below uint64 limits. No heap ordering or unstable container iteration.
    std::vector<std::uint64_t> distances(count + 1, infinity);
    std::vector<std::size_t> previous(count + 1, count + 1);
    std::vector<bool> visited(count + 1, false), reaches_goal(count, false);
    for (std::size_t i = 0; i < count; ++i) {
        if (clear(start, vertices_[i])) distances[i] = cost(start, vertices_[i]);
        reaches_goal[i] = clear(vertices_[i], goal);
    }
    for (std::size_t iteration = 0; iteration <= count; ++iteration) {
        auto current = count + 1;
        for (std::size_t i = 0; i <= count; ++i) {
            if (!visited[i] && distances[i] != infinity &&
                (current == count + 1 || distances[i] < distances[current])) current = i;
        }
        if (current == count + 1) return {};
        if (current == count) break;
        visited[current] = true;
        const auto relax = [&](std::size_t next, std::uint64_t edge_cost) {
            const auto candidate = distances[current] + edge_cost;
            if (!visited[next] && candidate < distances[next]) {
                distances[next] = candidate;
                previous[next] = current;
            }
        };
        for (const auto edge : edges_[current]) relax(edge.node, edge.cost);
        if (reaches_goal[current]) relax(count, cost(vertices_[current], goal));
    }
    if (distances[count] == infinity) return {};
    std::vector<Point> result{goal};
    auto cursor = previous[count];
    while (cursor < count) {
        if (vertices_[cursor] != start && vertices_[cursor] != goal)
            result.push_back(vertices_[cursor]);
        cursor = previous[cursor];
    }
    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace vf::nav
