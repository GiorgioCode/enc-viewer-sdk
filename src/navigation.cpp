// navigation.cpp — Escala física, celdas visibles, reglas e historial público de navegación.
// Mantiene operaciones de consulta sin contexto gráfico y valida antes de modificar la escena.
#include "detail/camera.h"
#include "detail/gdal_support.h"
#include "detail/mosaic.h"
#include "viewer_impl.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace enc {
Status Viewer::setPortrayalRules(const std::vector<PortrayalRule>& overrides) {
    std::set<std::string> names;
    auto rules = defaultPortrayalRules();
    for (const auto& r : overrides) {
        if (r.objectClass.empty() || !names.insert(r.objectClass).second ||
            !std::isfinite(r.lineWidth) || r.lineWidth < .25 || r.lineWidth > 8 || r.priority < 0 ||
            r.priority > 100 || (r.fill && *r.fill > 0xffffff) || (r.line && *r.line > 0xffffff) ||
            (r.category != DisplayCategory::Base && r.category != DisplayCategory::Standard &&
             r.category != DisplayCategory::Other) ||
            (r.pattern != LinePattern::Solid && r.pattern != LinePattern::Dashed &&
             r.pattern != LinePattern::Dotted))
            return detail::invalid("Regla de presentación inválida o duplicada.");
        auto it = std::find_if(rules.begin(), rules.end(),
                               [&](const auto& item) { return item.objectClass == r.objectClass; });
        if (it == rules.end())
            rules.push_back(r);
        else
            *it = r;
    }
    impl_->scene.rules = std::move(rules);
    return {};
}
std::vector<PortrayalRule> Viewer::portrayalRules() const {
    return impl_->scene.rules;
}
ScaleInfo Viewer::scaleInfo() const {
    const auto& s = impl_->scene;
    ScaleInfo out;
    out.metresPerPixel = detail::metresPerPixel(s);
    out.denominator = detail::displayScale(s);
    const double desired = std::max(1e-9, out.metresPerPixel * std::min(140., s.width * .25));
    const double decade = std::pow(10., std::floor(std::log10(desired)));
    const double normalized = desired / decade;
    out.barMetres = decade * (normalized >= 5 ? 5 : normalized >= 2 ? 2 : 1);
    out.barPixels = out.barMetres / out.metresPerPixel;
    for (const auto& c : visibleCells())
        out.overscale |= c.overscaleFactor > 1;
    return out;
}
std::vector<VisibleCell> Viewer::visibleCells() const {
    std::vector<VisibleCell> out;
    const auto& s = impl_->scene;
    if (!s.showCharts)
        return out;
    const auto b = detail::viewportBounds(s);
    for (auto i : detail::activeCells(s, 0)) {
        const auto& cell = s.cells[i];
        bool intersects = !cell.hasCoverage;
        for (const auto& p : cell.coverage)
            for (const auto& ring : p.rings) {
                if (ring.empty())
                    continue;
                double lo = ring[0].x, hi = lo, bot = 90, top = -90;
                for (auto v : ring) {
                    lo = std::min(lo, v.x);
                    hi = std::max(hi, v.x);
                    bot = std::min(bot, v.y);
                    top = std::max(top, v.y);
                }
                intersects |= Bounds{lo, bot, hi, top, true}.intersects(b);
            }
        if (intersects)
            out.push_back({cell.path, cell.compilationScale, cell.intendedUsage,
                           cell.compilationScale
                               ? double(cell.compilationScale) / detail::displayScale(s)
                               : 0});
    }
    return out;
}
DepthInfo Viewer::depthInfo() const {
    const auto& s = impl_->scene;
    DepthInfo result;
    result.requestedContour = s.presentation.safetyContour;
    if (!s.showCharts)
        return result;
    bool first = true;
    for (auto cell : detail::activeCells(s, 0)) {
        const auto contour = detail::safetyContour(s, static_cast<int>(cell));
        result.cells.push_back({s.cells[cell].path, contour});
        if (first) {
            result.displayedContour = contour;
            first = false;
        } else if (result.displayedContour != contour)
            result.displayedContour.reset();
    }
    return result;
}
Bounds Viewer::viewBounds(double margin) const {
    if (!std::isfinite(margin) || margin < 0)
        return {};
    return detail::viewportBounds(impl_->scene, margin);
}
void Viewer::rememberView() {
    auto& back = impl_->historyBack;
    back.push_back(camera());
    if (back.size() > 100)
        back.erase(back.begin());
    impl_->historyForward.clear();
}
bool Viewer::previousView() {
    auto& p = *impl_;
    if (p.historyBack.empty())
        return false;
    p.historyForward.push_back(camera());
    const auto c = p.historyBack.back();
    p.historyBack.pop_back();
    return bool(setCamera(c));
}
bool Viewer::nextView() {
    auto& p = *impl_;
    if (p.historyForward.empty())
        return false;
    p.historyBack.push_back(camera());
    const auto c = p.historyForward.back();
    p.historyForward.pop_back();
    return bool(setCamera(c));
}
Status Viewer::fitFeatures(const std::vector<FeatureId>& ids) {
    if (ids.empty())
        return detail::invalid("Se requiere al menos un objeto para encuadrar.");
    double lo = 0, hi = 0, bottom = 90, top = -90;
    bool first = true;
    for (auto id : ids) {
        const auto f = feature(id);
        if (!f)
            return {ErrorCode::NotFound, "Objeto inexistente o de otra generación.", {}};
        const double anchor = first ? f->bounds.west : lo;
        const double west = anchor + detail::wrapLongitude(f->bounds.west - anchor),
                     east = west + (f->bounds.east >= f->bounds.west
                                        ? f->bounds.east - f->bounds.west
                                        : f->bounds.east + 360 - f->bounds.west);
        lo = first ? west : std::min(lo, west);
        hi = first ? east : std::max(hi, east);
        first = false;
        bottom = std::min(bottom, f->bounds.south);
        top = std::max(top, f->bounds.north);
    }
    return fitBounds({detail::wrapLongitude(lo), bottom, detail::wrapLongitude(hi), top, true});
}
} // namespace enc
