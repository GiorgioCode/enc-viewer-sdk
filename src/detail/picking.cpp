// picking.cpp — Selección geográfica de objetos visibles mediante coordenadas de pantalla.
// Prioriza ayudas náuticas, líneas y finalmente áreas, respetando huecos y visibilidad.

#include "picking.h"
#include "camera.h"
#include "chart_style.h"
#include "geometry.h"
#include <algorithm>
#include <cmath>

namespace enc::detail {
int hitTest(const SceneState& app, double x, double y) {
    if (!app.showCharts)
        return -1;
    const double mapW = app.width - kSidebarWidth,
                 mapH = app.height - kToolbarHeight - kStatusHeight;
    Point world{(app.centerX + (x - mapW * 0.5) / app.zoom) / lonScale(app),
                app.centerLat - (y - kToolbarHeight - mapH * 0.5) / app.zoom};
    if (app.zoom < 800)
        return -1;
    // Los símbolos tienen prioridad sobre las áreas inferiores sin depender del orden
    // de capas de GDAL. La selección incluye el cuerpo dibujado por encima del anclaje.
    for (int pass = 0; pass < 2; ++pass)
        for (int i = static_cast<int>(app.features.size()) - 1; i >= 0; --i) {
            const auto& f = app.features[static_cast<std::size_t>(i)];
            if (f.points.empty() || !pointVisible(app, f) ||
                (isNavaid(f.symbol.kind) ? 0 : 1) != pass)
                continue;
            if (f.symbol.kind == nautical::Kind::Sounding &&
                (!app.presentation.showSoundings || app.zoom < 14000))
                continue;
            const double scale =
                std::clamp(app.zoom / 6500.0, 0.72, 1.15) * app.presentation.symbolScale;
            for (Point p : f.points) {
                p = toScreen(app, p);
                if (std::abs(p.x - x) < 10 * scale && y >= p.y - 30 * scale && y <= p.y + 9 * scale)
                    return i;
            }
        }
    for (int i = static_cast<int>(app.features.size()) - 1; i >= 0; --i) {
        const auto& f = app.features[static_cast<std::size_t>(i)];
        if (!app.layers[f.layer].visible || !inView(app, f))
            continue;
        for (const auto& line : f.lines)
            for (std::size_t j = 1; j < line.size(); ++j)
                if (segmentDistance({x, y}, toScreen(app, line[j - 1]), toScreen(app, line[j])) < 6)
                    return i;
    }
    for (int i = static_cast<int>(app.features.size()) - 1; i >= 0; --i) {
        const Feature& f = app.features[static_cast<std::size_t>(i)];
        if (!app.layers[f.layer].visible)
            continue;
        for (const Polygon& poly : f.polygons) {
            if (poly.rings.empty() || !inRing(world, poly.rings.front()))
                continue;
            bool hole = false;
            for (std::size_t r = 1; r < poly.rings.size(); ++r)
                if (inRing(world, poly.rings[r])) {
                    hole = true;
                    break;
                }
            if (!hole)
                return i;
        }
    }
    return -1;
}

} // namespace enc::detail
