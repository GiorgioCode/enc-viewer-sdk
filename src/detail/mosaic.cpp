// mosaic.cpp — Mosaico por cobertura y escala, categorías y representación condicional básica.
// Las celdas se ordenan de generales a detalladas; el último propietario de un punto prevalece.
#include "mosaic.h"
#include "camera.h"
#include "chart_style.h"
#include "geometry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <ogrsf_frmts.h>
namespace enc::detail {
// Los booleanos se calculan una vez por vista y margen, nunca por objeto ni por cuadro sin cambios.
struct MosaicCache {
    std::array<double, 12> key;
    struct Entry {
        double margin;
        std::vector<std::size_t> cells;
    };
    std::vector<Entry> entries;
};
namespace {
using Geometry = std::unique_ptr<OGRGeometry>;
double area(const OGRGeometry& geometry) {
    const auto type = wkbFlatten(geometry.getGeometryType());
    if (type == wkbPolygon)
        return geometry.toPolygon()->get_Area();
    if (OGR_GT_IsSubClassOf(type, wkbGeometryCollection)) {
        double result = 0;
        const auto* group = geometry.toGeometryCollection();
        for (int i = 0; i < group->getNumGeometries(); ++i)
            result += area(*group->getGeometryRef(i));
        return result;
    }
    return 0;
}
// Usa la misma proyección continua del dibujo: una celda sobre ±180° nunca cubre el centro del mundo.
Geometry screenCoverage(const SceneState& scene, const CellData& cell, double margin) {
    if (!cell.hasCoverage)
        return {};
    OGRMultiPolygon polygons;
    for (const auto& source : cell.coverage) {
        if (source.rings.empty() || source.rings.front().size() < 3)
            continue;
        const double anchor = source.rings.front().front().x;
        for (int copy = -1; copy <= 1; ++copy) {
            OGRPolygon polygon;
            for (const auto& sourceRing : source.rings) {
                if (sourceRing.size() < 3)
                    continue;
                OGRLinearRing ring;
                for (auto point : sourceRing) {
                    const auto screen = toScreenContinuous(scene, point, anchor, copy);
                    ring.addPoint(screen.x, screen.y);
                }
                ring.closeRings();
                polygon.addRing(&ring);
            }
            polygons.addGeometry(&polygon);
        }
    }
    if (polygons.IsEmpty())
        return std::make_unique<OGRPolygon>();
    Geometry coverage(polygons.UnionCascaded());
    if (!coverage)
        return {};
    OGRLinearRing boundary;
    boundary.addPoint(-margin, -margin);
    boundary.addPoint(scene.width + margin, -margin);
    boundary.addPoint(scene.width + margin, scene.height + margin);
    boundary.addPoint(-margin, scene.height + margin);
    boundary.closeRings();
    OGRPolygon viewport;
    viewport.addRing(&boundary);
    return Geometry(coverage->Intersection(&viewport));
}
bool coveredBy(const Geometry& geometry, const Geometry& coverage) {
    if (!geometry || !coverage)
        return false; // Ante geometría no utilizable se conserva la celda, sin inventar cobertura.
    Geometry remainder(geometry->Difference(coverage.get()));
    return remainder && area(*remainder) < 1e-5;
}
void extendCoverage(Geometry& combined, const Geometry& geometry) {
    if (geometry)
        combined.reset(combined ? combined->Union(geometry.get()) : geometry->clone());
}
} // namespace
double displayScale(const SceneState& s) {
    return metresPerPixel(s) / (s.presentation.millimetresPerPixel * .001);
}
const PortrayalRule* ruleFor(const SceneState& s, const std::string& name) {
    const auto it = std::find_if(s.rules.begin(), s.rules.end(),
                                 [&](const auto& r) { return r.objectClass == name; });
    return it == s.rules.end() ? nullptr : &*it;
}
std::vector<std::size_t> activeCells(const SceneState& s, double margin) {
    if (s.width <= 0 || s.height <= 0)
        return {};
    const std::array<double, 12> key{
        {s.centerX, s.centerLat, s.referenceLatitude, s.zoom, s.rotation, double(s.width),
         double(s.height), double(s.projection), s.presentation.millimetresPerPixel,
         s.presentation.detailThreshold, double(s.presentation.automaticMosaic),
         double(s.cells.size())}};
    if (!s.mosaicCache || s.mosaicCache->key != key) {
        s.mosaicCache = std::make_shared<MosaicCache>();
        s.mosaicCache->key = key;
    }
    for (const auto& entry : s.mosaicCache->entries)
        if (entry.margin == margin)
            return entry.cells;
    std::vector<std::size_t> ids;
    const auto region = viewportBounds(s, margin);
    for (std::size_t i = 0; i < s.cells.size(); ++i)
        if (!s.cells[i].bounds.valid || s.cells[i].bounds.intersects(region))
            ids.push_back(i);
    std::stable_sort(ids.begin(), ids.end(), [&](auto a, auto b) {
        const auto& x = s.cells[a];
        const auto& y = s.cells[b];
        if (x.compilationScale != y.compilationScale)
            return x.compilationScale > y.compilationScale;
        if (x.intendedUsage != y.intendedUsage)
            return x.intendedUsage < y.intendedUsage;
        return x.path.string() < y.path.string();
    });
    if (!ids.empty()) {
        std::vector<Geometry> coverage(s.cells.size());
        const auto scale = displayScale(s);
        const auto least = s.cells[ids.front()].compilationScale;
        std::vector<std::size_t> candidates;
        Geometry coarser;
        for (auto i : ids) {
            coverage[i] = screenCoverage(s, s.cells[i], margin);
            if (coverage[i] && area(*coverage[i]) < 1e-5)
                continue;
            const auto c = s.cells[i].compilationScale;
            const bool tooDetailed =
                c > 0 && c < least && scale > c * s.presentation.detailThreshold;
            // Una carta general vecina no debe ocultar la única cobertura de otra región.
            if (s.presentation.automaticMosaic && tooDetailed && coveredBy(coverage[i], coarser))
                continue;
            candidates.push_back(i);
            extendCoverage(coarser, coverage[i]);
        }
        ids.clear();
        Geometry finer;
        for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
            const auto i = *it;
            // Las generales totalmente tapadas no provocan falsas advertencias de sobreescala.
            if (s.presentation.automaticMosaic && coveredBy(coverage[i], finer))
                continue;
            ids.push_back(i);
            extendCoverage(finer, coverage[i]);
        }
        std::reverse(ids.begin(), ids.end());
    }
    if (s.mosaicCache->entries.size() >= 4)
        s.mosaicCache->entries.erase(s.mosaicCache->entries.begin());
    s.mosaicCache->entries.push_back({margin, ids});
    return ids;
}
bool cellContains(const CellData& cell, Point p) {
    if (!cell.hasCoverage)
        return true;
    for (const auto& polygon : cell.coverage) {
        if (polygon.rings.empty() || polygon.rings.front().empty())
            continue;
        p.x = polygon.rings[0][0].x + wrapLongitude(p.x - polygon.rings[0][0].x);
        if (!inRing(p, polygon.rings[0]))
            continue;
        bool hole = false;
        for (std::size_t r = 1; r < polygon.rings.size(); ++r)
            hole |= inRing(p, polygon.rings[r]);
        if (!hole)
            return true;
    }
    return false;
}
bool visibleAt(const SceneState& s, std::size_t cell, Point p) {
    if (!s.presentation.automaticMosaic)
        return true;
    const auto cells = activeCells(s);
    bool found = false;
    for (auto i : cells) {
        if (i == cell)
            found = true;
        else if (found && cellContains(s.cells[i], p))
            return false;
    }
    return found && cellContains(s.cells[cell], p);
}
bool featureVisible(const SceneState& s, const Feature& f, bool selected) {
    if (!s.layers[f.layer].visible || !inView(s, f) ||
        (s.renderingCell >= 0 && f.cell != static_cast<std::size_t>(s.renderingCell)))
        return false;
    const auto* rule = ruleFor(s, s.layers[f.layer].objectClass);
    const auto category = rule ? rule->category : DisplayCategory::Other;
    if (s.presentation.displayMode == DisplayMode::Base && category != DisplayCategory::Base)
        return false;
    if (s.presentation.displayMode == DisplayMode::Standard && category == DisplayCategory::Other)
        return false;
    if (!selected && category != DisplayCategory::Base && s.presentation.respectScale) {
        const double scale = displayScale(s);
        if ((f.minScale > 0 && scale > f.minScale) || (f.maxScale > 0 && scale < f.maxScale))
            return false;
    }
    return true;
}
bool dangerous(const SceneState& s, const Feature& f) {
    const auto& name = s.layers[f.layer].objectClass;
    if (name != "WRECKS" && name != "OBSTRN" && name != "UWTROC")
        return false;
    const auto depth = attr(f, "VALSOU");
    const auto water = attr(f, "WATLEV");
    return water != "2" &&
           (depth.empty() || std::atof(depth.c_str()) <= s.presentation.safetyDepth);
}
std::optional<double> safetyContour(const SceneState& s, int cell) {
    std::optional<double> out;
    for (const auto& f : s.features)
        if (s.layers[f.layer].objectClass == "DEPCNT" &&
            (cell < 0 || f.cell == static_cast<std::size_t>(cell))) {
            const auto text = attr(f, "VALDCO");
            if (text.empty())
                continue;
            const double value = std::atof(text.c_str());
            if (value >= s.presentation.safetyContour && (!out || value < *out))
                out = value;
        }
    return out;
}
} // namespace enc::detail
