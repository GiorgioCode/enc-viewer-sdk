// radar_scene.cpp — Construcción de la escena de radar a partir de las cartas.
// Elige un origen sobre agua, muestrea costas y estructuras y ubica los blancos sintéticos.
// Contiene la implementación de esta responsabilidad.

#include "radar_scene.h"
#include "camera.h"
#include "chart_style.h"
#include <algorithm>
#include <cmath>

namespace enc::detail {

// Busca el centro de un triángulo de profundidad cercano al punto pedido.
overlays::GeoPoint nearestWater(const SceneState& app, overlays::GeoPoint desired) {
    auto result = desired;
    double best = std::numeric_limits<double>::infinity();
    for (const auto& f : app.features) {
        const auto& name = app.layers[f.layer].objectClass;
        if (name != "DEPARE" && name != "DRGARE")
            continue;
        for (const auto& polygon : f.polygons)
            for (std::size_t i = 0; i + 2 < polygon.triangles.size(); i += 3) {
                const auto& a = polygon.triangles[i];
                const auto& b = polygon.triangles[i + 1];
                const auto& c = polygon.triangles[i + 2];
                const overlays::GeoPoint p{(a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3};
                const double distance =
                    std::hypot((p.lon - desired.lon) * lonScale(app), p.lat - desired.lat);
                if (distance < best) {
                    best = distance;
                    result = p;
                }
            }
    }
    return result;
}

// Muestrea costa y estructuras de la carta y ubica los blancos sintéticos del radar.
void buildRadarScene(SceneState& app) {
    auto& radar = app.radar;
    radar.clearScene();
    const auto trace = [&](const std::vector<Point>& line) {
        for (std::size_t i = 1; i < line.size(); ++i)
            radar.addSegment({line[i - 1].x, line[i - 1].y}, {line[i].x, line[i].y}, .8f);
    };
    const bool hasCoastline =
        std::any_of(app.layers.begin(), app.layers.end(),
                    [](const Layer& layer) { return layer.objectClass == "COALNE"; });
    for (const auto& f : app.features) {
        const auto& name = app.layers[f.layer].objectClass;
        if ((name == "LNDARE" && !hasCoastline) || name == "COALNE" || name == "SLCONS" ||
            name == "BRIDGE" || name == "BUISGL") {
            for (const auto& line : f.lines)
                trace(line);
            for (const auto& polygon : f.polygons)
                for (const auto& ring : polygon.rings)
                    trace(ring);
        } else if (isNavaid(f.symbol.kind))
            for (const auto& p : f.points)
                radar.addEcho(radar.local({p.x, p.y}), .95f);
    }
    for (std::size_t i = 0; i < radar.targets.size(); ++i) {
        const double angle = 1.3 + i * 2.0;
        const auto desired = radar.geographic(
            {radar.rangeNm * .4 * std::sin(angle), radar.rangeNm * .4 * std::cos(angle)});
        auto local = radar.local(nearestWater(app, desired));
        if (std::hypot(local.east, local.north) > radar.rangeNm * .9)
            local = {radar.rangeNm * .4 * std::sin(angle), radar.rangeNm * .4 * std::cos(angle)};
        radar.targets[i] = local;
    }
}

// Selecciona un origen inicial sobre agua y un alcance adecuado a la vista actual.
void initializeRadar(SceneState& app) {
    auto& radar = app.radar;
    radar.origin = nearestWater(
        app, worldAt(app, (app.width - kSidebarWidth) * .5,
                     kToolbarHeight + (app.height - kToolbarHeight - kStatusHeight) * .5));
    const double desired =
        std::min(app.width - kSidebarWidth, app.height - kToolbarHeight - kStatusHeight) * .36 /
        app.zoom * 60;
    radar.rangeNm = *std::min_element(
        overlays::radarRanges.begin(), overlays::radarRanges.end(),
        [&](double a, double b) { return std::abs(a - desired) < std::abs(b - desired); });
    radar.seconds = 0;
    radar.initialized = true;
    buildRadarScene(app);
}

// Cambia el alcance y vuelve a muestrear los ecos que quedan dentro del círculo.
void setRadarRange(SceneState& app, int direction) {
    app.radar.changeRange(direction);
    if (app.radar.initialized)
        buildRadarScene(app);
}

} // namespace enc::detail
