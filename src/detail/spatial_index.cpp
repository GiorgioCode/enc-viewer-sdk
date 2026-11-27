// spatial_index.cpp — Construcción por mediana y consulta jerárquica de envolventes geográficas.
// Los resultados se deduplican para no repetir objetos cerca de la discontinuidad de longitud.
#include "spatial_index.h"
#include <algorithm>
#include <functional>
#include <numeric>
namespace enc::detail {
int SpatialIndex::build(const std::vector<Feature>& fs, std::size_t begin, std::size_t end) {
    Node n{1e30, 1e30, -1e30, -1e30, -1, -1, begin, end};
    for (auto i = begin; i < end; ++i) {
        const auto& f = fs[ids_[i]];
        n.x1 = std::min(n.x1, f.minX);
        n.x2 = std::max(n.x2, f.maxX);
        n.y1 = std::min(n.y1, f.minY);
        n.y2 = std::max(n.y2, f.maxY);
    }
    const int index = static_cast<int>(nodes_.size());
    nodes_.push_back(n);
    if (end - begin > 16) {
        const auto mid = begin + (end - begin) / 2;
        const bool x = n.x2 - n.x1 > n.y2 - n.y1;
        std::nth_element(ids_.begin() + begin, ids_.begin() + mid, ids_.begin() + end,
                         [&](auto a, auto b) {
                             return x ? fs[a].minX + fs[a].maxX < fs[b].minX + fs[b].maxX
                                      : fs[a].minY + fs[a].maxY < fs[b].minY + fs[b].maxY;
                         });
        const auto left = build(fs, begin, mid), right = build(fs, mid, end);
        nodes_[index].left = left;
        nodes_[index].right = right;
    }
    return index;
}
void SpatialIndex::rebuild(const std::vector<Feature>& features) {
    nodes_.clear();
    ids_.resize(features.size());
    std::iota(ids_.begin(), ids_.end(), 0);
    if (!ids_.empty())
        build(features, 0, ids_.size());
}
std::vector<std::size_t> SpatialIndex::query(const Bounds& region) const {
    std::vector<std::size_t> out;
    if (nodes_.empty())
        return out;
    if (!region.valid) {
        out = ids_;
        std::sort(out.begin(), out.end());
        return out;
    }
    double east = region.east;
    if (east < region.west)
        east += 360;
    for (int shift = -1; shift <= 1; ++shift) {
        const double west = region.west + 360 * shift, right = east + 360 * shift;
        std::function<void(int)> visit = [&](int i) {
            const auto& n = nodes_[i];
            if (n.x2 < west || n.x1 > right || n.y2 < region.south || n.y1 > region.north)
                return;
            if (n.left < 0)
                out.insert(out.end(), ids_.begin() + n.begin, ids_.begin() + n.end);
            else {
                visit(n.left);
                visit(n.right);
            }
        };
        visit(0);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}
} // namespace enc::detail
