#pragma once
#include "navigation.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace vf {
// Rebuilt from tick-start positions. Each entity occurs in one bucket; queries
// return ascending IDs so broadphase traversal cannot change authoritative ties.
// Callers expand queries by the maximum displacement when testing current sweeps.
class SpatialIndex {
public:
    SpatialIndex(int width, int height, int cell_size)
        : width_(width), height_(height), cell_size_(cell_size) {
        if (width < 1 || width > 128 || height < 1 || height > 128 || cell_size < 1 || cell_size > 256)
            throw std::invalid_argument("unsupported spatial dimensions");
        heads_.resize(static_cast<size_t>(width) * height,0);
        entries_.reserve(500);
    }
    void insert(uint32_t id, nav::Point p) {
        if (!id || p.x < 0 || p.z < 0 || p.x >= width_*cell_size_ || p.z >= height_*cell_size_)
            throw std::invalid_argument("spatial point outside bounds");
        auto& head=heads_[static_cast<size_t>(p.z/cell_size_)*width_+p.x/cell_size_];
        entries_.push_back({id,head});
        head=entries_.size();
    }
    void query(nav::Rect region, std::vector<uint32_t>& result) const {
        result.clear();
        if (region.max_x < 0 || region.max_z < 0 || region.min_x >= width_*cell_size_ ||
            region.min_z >= height_*cell_size_ || region.min_x > region.max_x || region.min_z > region.max_z) return;
        const int x0=std::clamp(region.min_x,0,width_*cell_size_-1)/cell_size_;
        const int z0=std::clamp(region.min_z,0,height_*cell_size_-1)/cell_size_;
        const int x1=std::clamp(region.max_x,0,width_*cell_size_-1)/cell_size_;
        const int z1=std::clamp(region.max_z,0,height_*cell_size_-1)/cell_size_;
        for (int z=z0; z<=z1; ++z) for (int x=x0; x<=x1; ++x) {
            auto entry=heads_[static_cast<size_t>(z)*width_+x];
            while (entry) {
                const auto& node=entries_[entry-1];
                result.push_back(node.id); entry=node.next;
            }
        }
        std::sort(result.begin(),result.end());
    }
private:
    int width_, height_, cell_size_;
    struct Entry { uint32_t id; size_t next; };
    std::vector<size_t> heads_;
    std::vector<Entry> entries_;
};
}
