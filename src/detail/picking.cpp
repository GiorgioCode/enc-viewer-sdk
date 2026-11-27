// picking.cpp — Consulta de todos los objetos bajo el cursor, ordenados por prioridad y distancia.
// Comparte escala, categorías y cobertura con el dibujo; las áreas respetan sus huecos.
#include "picking.h"
#include "camera.h"
#include "chart_style.h"
#include "geometry.h"
#include "mosaic.h"
#include <algorithm>
#include <cmath>
namespace enc::detail {
std::vector<std::size_t> hitTests(const SceneState& s, double x, double y, double tolerance) {
    if (!s.showCharts || s.zoom < 800)
        return {};
    const auto w = worldAt(s, x, y);
    const double scale = std::clamp(s.zoom / 6500., .72, 1.15) * s.presentation.symbolScale;
    // La envolvente debe incluir todos los anclajes cuyo símbolo puede cubrir el cursor.
    // Una única diagonal pierde una componente a 45°; los cuatro vértices conservan la rotación.
    const double reach = std::max({tolerance, 35., 30 * scale});
    double dx = 0, dy = 0;
    for (double sx : {-1., 1.})
        for (double sy : {-1., 1.}) {
            const auto corner = worldAt(s, x + sx * reach, y + sy * reach);
            dx = std::max(dx, std::abs(wrapLongitude(corner.lon - w.lon)));
            dy = std::max(dy, std::abs(corner.lat - w.lat));
        }
    dx += 1e-8;
    dy += 1e-8;
    const auto ids = s.spatial.query(
        {wrapLongitude(w.lon - dx), w.lat - dy, wrapLongitude(w.lon + dx), w.lat + dy, true});
    struct Hit {
        std::size_t index;
        int rank, priority;
        double distance;
    };
    std::vector<Hit> hits;
    for (auto i : ids) {
        const auto& f = s.features[i];
        if (!featureVisible(s, f) || !visibleAt(s, f.cell, {w.lon, w.lat}))
            continue;
        int rank = 3;
        double distance = 1e30;
        if (pointVisible(s, f) && !(f.symbol.kind == nautical::Kind::Sounding &&
                                    (!s.presentation.showSoundings || s.zoom < 14000)))
            for (auto p : f.points) {
                p = toScreen(s, p);
                if (std::abs(p.x - x) < std::max(tolerance, 10 * scale) && y >= p.y - 30 * scale &&
                    y <= p.y + 9 * scale) {
                    rank = isNavaid(f.symbol.kind) ? 0 : 1;
                    distance = std::min(distance, std::hypot(p.x - x, p.y - y));
                }
            }
        if (rank == 3)
            for (const auto& line : f.lines)
                for (int copy = -1; copy <= 1; ++copy)
                    for (std::size_t j = 1; j < line.size(); ++j) {
                        const auto d = segmentDistance(
                            {x, y}, toScreenContinuous(s, line[j - 1], line.front().x, copy),
                            toScreenContinuous(s, line[j], line.front().x, copy));
                        if (d <= tolerance) {
                            rank = 2;
                            distance = std::min(distance, d);
                        }
                    }
        if (rank == 3)
            for (const auto& polygon : f.polygons) {
                if (polygon.rings.empty() || polygon.rings.front().empty())
                    continue;
                Point world{polygon.rings[0][0].x + wrapLongitude(w.lon - polygon.rings[0][0].x),
                            w.lat};
                if (!inRing(world, polygon.rings[0]))
                    continue;
                bool hole = false;
                for (std::size_t r = 1; r < polygon.rings.size(); ++r)
                    hole |= inRing(world, polygon.rings[r]);
                if (!hole) {
                    rank = 2;
                    distance = 1e20;
                }
            }
        if (distance < 1e30) {
            const auto* rule = ruleFor(s, s.layers[f.layer].objectClass);
            hits.push_back({i, rank, rule ? rule->priority : 5, distance});
        }
    }
    std::stable_sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) {
        if (a.rank != b.rank)
            return a.rank < b.rank;
        if (a.priority != b.priority)
            return a.priority > b.priority;
        if (a.distance != b.distance)
            return a.distance < b.distance;
        return a.index > b.index;
    });
    std::vector<std::size_t> result;
    for (const auto& hit : hits)
        result.push_back(hit.index);
    return result;
}
int hitTest(const SceneState& s, double x, double y) {
    const auto hits = hitTests(s, x, y, 6);
    return hits.empty() ? -1 : static_cast<int>(hits.front());
}
} // namespace enc::detail
