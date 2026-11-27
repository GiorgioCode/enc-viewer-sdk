// camera.cpp — Proyecciones local/Mercator, rotación, cruce de longitud y navegación al cursor.
// Todas las operaciones usan píxeles lógicos; la aplicación conserva el contexto y la presentación física.
#include "camera.h"
#include <algorithm>
#include <cmath>
namespace enc::detail {
namespace {
double projectedY(const SceneState& s, double lat) {
    if (s.projection == Projection::LocalEquirectangular)
        return lat;
    lat = std::clamp(lat, -85.05112878, 85.05112878);
    return std::log(std::tan(kPi / 4 + lat * kPi / 360)) * 180 / kPi;
}
double latitude(const SceneState& s, double y) {
    return s.projection == Projection::LocalEquirectangular
               ? std::clamp(y, -90., 90.)
               : (2 * std::atan(std::exp(std::clamp(y, -180., 180.) * kPi / 180)) - kPi / 2) * 180 /
                     kPi;
}
// Complementa el mayor hueco circular: una huella próxima a ±180° conserva su ancho real.
std::pair<double, double> longitudeArc(std::vector<double> xs) {
    std::sort(xs.begin(), xs.end());
    double gap = -1, start = xs.front(), end = xs.back();
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const double next = i + 1 < xs.size() ? xs[i + 1] : xs[0] + 360;
        if (next - xs[i] > gap) {
            gap = next - xs[i];
            start = std::fmod(next, 360.);
            end = xs[i];
        }
    }
    return {start - 180, end - 180};
}
} // namespace
double wrapLongitude(double lon) {
    return std::remainder(lon, 360.);
}
double lonScale(const SceneState& s) {
    return s.projection == Projection::Mercator
               ? 1
               : std::max(.17, std::cos(s.referenceLatitude * kPi / 180));
}
std::pair<double, double> framebufferScale(const SceneState& s) {
    return {double(s.framebufferWidth) / std::max(1, s.width),
            double(s.framebufferHeight) / std::max(1, s.height)};
}
Point toScreen(const SceneState& s, Point p) {
    const double dx = wrapLongitude(p.x - s.centerX / lonScale(s)) * lonScale(s) * s.zoom;
    const double dy = (projectedY(s, s.centerLat) - projectedY(s, p.y)) * s.zoom;
    const double angle = s.rotation * kPi / 180, c = std::cos(angle), sn = std::sin(angle);
    return {s.width * .5 + c * dx - sn * dy, s.height * .5 + sn * dx + c * dy, p.depth};
}
Point toScreenContinuous(const SceneState& s, Point p, double anchorLongitude, int worldCopy) {
    const double dx = (wrapLongitude(anchorLongitude - s.centerX / lonScale(s)) +
                       (p.x - anchorLongitude) + 360. * worldCopy) *
                      lonScale(s) * s.zoom;
    const double dy = (projectedY(s, s.centerLat) - projectedY(s, p.y)) * s.zoom;
    const double angle = s.rotation * kPi / 180, c = std::cos(angle), sn = std::sin(angle);
    return {s.width * .5 + c * dx - sn * dy, s.height * .5 + sn * dx + c * dy, p.depth};
}
overlays::GeoPoint worldAt(const SceneState& s, double x, double y) {
    const double angle = s.rotation * kPi / 180, c = std::cos(angle), sn = std::sin(angle);
    x -= s.width * .5;
    y -= s.height * .5;
    const double dx = c * x + sn * y, dy = -sn * x + c * y;
    return {wrapLongitude((s.centerX + dx / s.zoom) / lonScale(s)),
            latitude(s, projectedY(s, s.centerLat) - dy / s.zoom)};
}
Bounds viewportBounds(const SceneState& s, double margin) {
    double lo = 1e30, hi = -1e30, bottom = 90, top = -90;
    const double centre = s.centerX / lonScale(s);
    for (const auto& p :
         std::array<std::pair<double, double>, 4>{{{-margin, -margin},
                                                   {s.width + margin, -margin},
                                                   {s.width + margin, s.height + margin},
                                                   {-margin, s.height + margin}}}) {
        const auto g = worldAt(s, p.first, p.second);
        const double lon = centre + wrapLongitude(g.lon - centre);
        lo = std::min(lo, lon);
        hi = std::max(hi, lon);
        bottom = std::min(bottom, g.lat);
        top = std::max(top, g.lat);
    }
    // Una vista mundial abarca todo el dominio, sin plegar sus esquinas sobre el centro.
    const double angle = s.rotation * kPi / 180;
    if ((std::abs(std::cos(angle)) * (s.width + 2 * margin) +
         std::abs(std::sin(angle)) * (s.height + 2 * margin)) /
            s.zoom / lonScale(s) >=
        360)
        return {-180, bottom, 180, top, true};
    return {wrapLongitude(lo), bottom, wrapLongitude(hi), top, true};
}
bool fitRegion(SceneState& s, const Bounds& b) {
    if (!b.valid)
        return false;
    const double width = b.east >= b.west ? b.east - b.west : b.east + 360 - b.west;
    s.referenceLatitude = std::clamp((b.south + b.north) * .5, -80., 80.);
    s.centerX = wrapLongitude(b.west + width * .5) * lonScale(s);
    s.centerLat = latitude(s, (projectedY(s, b.south) + projectedY(s, b.north)) * .5);
    const double w = std::max(1e-8, width * lonScale(s));
    const double h = std::max(1e-8, projectedY(s, b.north) - projectedY(s, b.south));
    const double a = s.rotation * kPi / 180, c = std::abs(std::cos(a)), sn = std::abs(std::sin(a));
    s.zoom = std::clamp(std::min(s.width / (w * c + h * sn), s.height / (w * sn + h * c)) * .88,
                        .00001, 1e7);
    return true;
}
bool fitChart(SceneState& s) {
    if (!s.hasBounds)
        return false;
    // Busca el arco mínimo que contiene los datos; evita encuadrar 358° para celdas vecinas a ±180°.
    std::vector<double> xs;
    for (const auto& f : s.features)
        if (!f.points.empty() || !f.lines.empty() || !f.polygons.empty()) {
            xs.push_back(wrapLongitude(f.minX) + 180);
            xs.push_back(wrapLongitude(f.maxX) + 180);
        }
    if (xs.empty())
        return false;
    const auto [west, east] = longitudeArc(std::move(xs));
    return fitRegion(s, {west, s.minLat, east, s.maxLat, true});
}
void fitWorld(SceneState& s) {
    s.rotation = 0;
    fitRegion(s, {-180, -85, 180, 85, true});
}
void fitRaster(SceneState& s, const overlays::Raster& r) {
    if (!r.loaded())
        return;
    double bottom = 90, top = -90;
    std::vector<double> xs;
    for (int i = 0; i < 4; ++i) {
        const auto p = r.placedCorner(i);
        xs.push_back(wrapLongitude(p.lon) + 180);
        bottom = std::min(bottom, p.lat);
        top = std::max(top, p.lat);
    }
    const auto [west, east] = longitudeArc(std::move(xs));
    fitRegion(s, {west, bottom, east, top, true});
}
bool inView(const SceneState& s, const Feature& f, double margin) {
    if (f.maxX - f.minX > 180)
        return true;
    double lo = 1e30, hi = -1e30, top = 1e30, bottom = -1e30;
    for (Point p : std::array<Point, 4>{
             {{f.minX, f.minY}, {f.maxX, f.minY}, {f.maxX, f.maxY}, {f.minX, f.maxY}}}) {
        p = toScreen(s, p);
        lo = std::min(lo, p.x);
        hi = std::max(hi, p.x);
        top = std::min(top, p.y);
        bottom = std::max(bottom, p.y);
    }
    return hi >= -margin && lo <= s.width + margin && bottom >= -margin && top <= s.height + margin;
}
void panBy(SceneState& s, double dx, double dy) {
    const auto p = worldAt(s, s.width * .5 - dx, s.height * .5 - dy);
    s.centerLat = p.lat;
    s.centerX = p.lon * lonScale(s);
}
void zoomAt(SceneState& s, double x, double y, double factor) {
    const auto old = worldAt(s, x, y);
    s.zoom = std::clamp(s.zoom * factor, .00001, 1e7);
    const auto now = toScreen(s, {old.lon, old.lat});
    panBy(s, x - now.x, y - now.y);
}
double metresPerPixel(const SceneState& s) {
    const double lat = s.centerLat * kPi / 180, a = s.rotation * kPi / 180;
    const double horizontal = std::cos(a) * std::cos(lat) / lonScale(s);
    const double vertical =
        std::sin(a) * (s.projection == Projection::Mercator ? std::cos(lat) : 1);
    return 111319.49079327358 / s.zoom * std::hypot(horizontal, vertical);
}
} // namespace enc::detail
