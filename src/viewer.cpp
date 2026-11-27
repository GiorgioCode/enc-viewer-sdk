// viewer.cpp — Gestión de escenas, cámara y selección a través de la API pública.
// Las cargas se preparan en CPU y se confirman juntas; un fallo conserva la escena anterior.
#include "detail/camera.h"
#include "detail/chart_reader.h"
#include "detail/chart_style.h"
#include "detail/gdal_support.h"
#include "detail/mosaic.h"
#include "detail/picking.h"
#include "detail/radar_scene.h"
#include "viewer_impl.h"
#include "world_data.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace enc {
namespace detail {
bool finite(double value) {
    return std::isfinite(value);
}
bool validGeo(enc::GeoPoint point) {
    return finite(point.lon) && finite(point.lat) && std::abs(point.lon) <= 180 &&
           std::abs(point.lat) <= 90;
}
} // namespace detail
Viewer::Impl::Impl() {
    startup = initialize();
    if (startup && !detail::loadBasemap(scene, kEmbeddedWorld))
        startup = {ErrorCode::Decode, "No se pudo abrir el mapa mundial incorporado.", {}};
    detail::fitWorld(scene);
}
Viewer::Viewer() : impl_(std::make_unique<Impl>()) {}
Viewer::~Viewer() = default;
Viewer::Viewer(Viewer&&) noexcept = default;
Viewer& Viewer::operator=(Viewer&&) noexcept = default;

Status Viewer::loadFolder(const std::filesystem::path& source, const LoadOptions& options) {
    Catalog catalog;
    ScanOptions scan;
    scan.inspectMetadata = false;
    auto status = catalog.scan(source, scan);
    if (!status)
        return status;
    auto loaded = loadCatalog(catalog, options);
    loaded.diagnostics.insert(loaded.diagnostics.begin(), status.diagnostics.begin(),
                              status.diagnostics.end());
    return loaded;
}
Status Viewer::loadCatalog(const Catalog& catalog, const LoadOptions& options) {
    std::vector<std::filesystem::path> paths;
    for (const auto& cell : catalog.cells())
        paths.push_back(cell.path);
    return loadCells(paths, options);
}
Status Viewer::loadCells(const std::vector<std::filesystem::path>& cells,
                         const LoadOptions& options) {
    if (!impl_->startup)
        return impl_->startup;
    if (cells.empty())
        return detail::invalid("No hay celdas .000 para cargar.");
    detail::SceneState candidate;
    std::set<std::filesystem::path> seen;
    Status status;
    std::size_t completed = 0;
    for (const auto& path : cells) {
        if (options.progress && !options.progress({completed, cells.size(), path}))
            return {ErrorCode::Cancelled, "Carga cancelada; se conserva la escena anterior.", {}};
        std::error_code error;
        const auto canonical = std::filesystem::weakly_canonical(path, error);
        if (error)
            return {ErrorCode::Io, "Ruta no accesible: " + path.string(), {}};
        if (seen.insert(canonical).second) {
            // Aísla cada celda para no incorporar geometría parcial ante un error del controlador.
            detail::SceneState single;
            detail::GdalMessages messages(canonical);
            const bool read = detail::addCell(single, canonical) && single.hasBounds;
            const bool decodeError =
                std::any_of(messages.messages.begin(), messages.messages.end(),
                            [](const auto& d) { return d.severity == Severity::Error; });
            status.diagnostics.insert(status.diagnostics.end(), messages.messages.begin(),
                                      messages.messages.end());
            if (!read || decodeError) {
                status.diagnostics.push_back(
                    {Severity::Error, canonical,
                     "Celda no incorporada; revisar formato, geometría y actualizaciones."});
                if (!options.allowPartial) {
                    status.code = ErrorCode::Decode;
                    status.message = "Falló la lectura de " + canonical.string();
                    return status;
                }
            } else {
                const auto offset = candidate.layers.size();
                const auto cellOffset = candidate.cells.size();
                for (auto& cell : single.cells)
                    candidate.cells.push_back(std::move(cell));
                for (auto& feature : single.features) {
                    feature.layer += offset;
                    feature.cell += cellOffset;
                    candidate.features.push_back(std::move(feature));
                }
                for (auto& layer : single.layers)
                    candidate.layers.push_back(std::move(layer));
                if (!candidate.hasBounds) {
                    candidate.minLon = single.minLon;
                    candidate.maxLon = single.maxLon;
                    candidate.minLat = single.minLat;
                    candidate.maxLat = single.maxLat;
                    candidate.hasBounds = true;
                } else {
                    candidate.minLon = std::min(candidate.minLon, single.minLon);
                    candidate.maxLon = std::max(candidate.maxLon, single.maxLon);
                    candidate.minLat = std::min(candidate.minLat, single.minLat);
                    candidate.maxLat = std::max(candidate.maxLat, single.maxLat);
                }
                candidate.indexedCells.push_back(canonical);
            }
        }
        ++completed;
    }
    if (!candidate.hasBounds) {
        status.code = ErrorCode::Decode;
        status.message = "Ninguna celda contiene geometría utilizable.";
        return status;
    }
    if (options.progress && !options.progress({completed, cells.size(), {}}))
        return {ErrorCode::Cancelled, "Carga cancelada antes de confirmar la escena.", {}};
    detail::prepareSymbols(candidate);
    candidate.spatial.rebuild(candidate.features);
    impl_->commit(std::move(candidate), options.fitView);
    status.message = std::to_string(impl_->scene.indexedCells.size()) + " celdas cargadas.";
    return status;
}
void Viewer::Impl::commit(detail::SceneState&& candidate, bool fitView) {
    // Sólo transfiere datos ENC; cámara, estilos y capas externas conservan su propietario.
    auto& s = scene;
    s.layers = std::move(candidate.layers);
    s.features = std::move(candidate.features);
    s.cells = std::move(candidate.cells);
    s.mosaicCache.reset();
    s.spatial = std::move(candidate.spatial);
    s.indexedCells = std::move(candidate.indexedCells);
    s.minLon = candidate.minLon;
    s.maxLon = candidate.maxLon;
    s.minLat = candidate.minLat;
    s.maxLat = candidate.maxLat;
    s.hasBounds = candidate.hasBounds;
    s.selected = -1;
    s.labelPositions.clear();
    if (++generation == 0)
        ++generation;
    if (fitView)
        detail::fitChart(s);
    if (s.radar.initialized && !s.radar.external)
        detail::buildRadarScene(s);
}

void Viewer::clearCharts() {
    auto& scene = impl_->scene;
    scene.layers.clear();
    scene.features.clear();
    scene.indexedCells.clear();
    scene.cells.clear();
    scene.mosaicCache.reset();
    scene.spatial.rebuild(scene.features);
    scene.hasBounds = false;
    scene.selected = -1;
    if (++impl_->generation == 0)
        ++impl_->generation;
    if (scene.radar.initialized && !scene.radar.external)
        detail::buildRadarScene(scene);
}
SceneInfo Viewer::sceneInfo() const {
    const auto& s = impl_->scene;
    SceneInfo result{s.indexedCells.size(),
                     s.layers.size(),
                     s.features.size(),
                     0,
                     {s.minLon, s.minLat, s.maxLon, s.maxLat, s.hasBounds}};
    for (const auto& f : s.features)
        for (const auto& polygon : f.polygons)
            result.triangleCount += polygon.triangles.size() / 3;
    return result;
}
std::vector<LayerInfo> Viewer::layers() const {
    std::vector<LayerInfo> result;
    const auto& s = impl_->scene;
    for (std::size_t i = 0; i < s.layers.size(); ++i) {
        const auto& l = s.layers[i];
        result.push_back({i, l.count, l.name, l.objectClass, l.cellKey, l.visible});
    }
    return result;
}
Status Viewer::setLayerVisible(std::size_t layer, bool visible) {
    auto& s = impl_->scene;
    if (layer >= s.layers.size())
        return {ErrorCode::NotFound, "Capa ENC inexistente.", {}};
    s.layers[layer].visible = visible;
    if (!visible && s.selected >= 0 &&
        s.features[static_cast<std::size_t>(s.selected)].layer == layer)
        s.selected = -1;
    return {};
}
void Viewer::setChartsVisible(bool visible) {
    impl_->scene.showCharts = visible;
    if (!visible)
        impl_->scene.selected = -1;
}
bool Viewer::chartsVisible() const {
    return impl_->scene.showCharts;
}
void Viewer::setTheme(Theme theme) {
    impl_->scene.nightMode = theme == Theme::Night;
}
Theme Viewer::theme() const {
    return impl_->scene.nightMode ? Theme::Night : Theme::Day;
}
Status Viewer::setPresentation(const Presentation& p) {
    if (!detail::finite(p.shallowDepth) || !detail::finite(p.safetyDepth) ||
        !detail::finite(p.deepDepth) || p.shallowDepth < 0 || p.shallowDepth > p.safetyDepth ||
        p.safetyDepth > p.deepDepth || !detail::finite(p.symbolScale) || p.symbolScale < .25 ||
        p.symbolScale > 4 || !detail::finite(p.textScale) || p.textScale < .5 || p.textScale > 3 ||
        !detail::finite(p.labelDensity) || p.labelDensity < .1 || p.labelDensity > 4 ||
        !detail::finite(p.safetyContour) || p.safetyContour < 0 ||
        !detail::finite(p.millimetresPerPixel) || p.millimetresPerPixel < .01 ||
        p.millimetresPerPixel > 10 || !detail::finite(p.detailThreshold) || p.detailThreshold < 1 ||
        p.detailThreshold > 100 ||
        (p.displayMode != DisplayMode::Base && p.displayMode != DisplayMode::Standard &&
         p.displayMode != DisplayMode::All))
        return detail::invalid("Presentación inválida: revisar profundidades, escalas, densidad y "
                               "calibración de píxeles.");
    impl_->scene.presentation = p;
    return {};
}
Presentation Viewer::presentation() const {
    return impl_->scene.presentation;
}
Status Viewer::resize(int width, int height) {
    if (width < 0 || height < 0)
        return detail::invalid("El tamaño lógico no puede ser negativo.");
    impl_->scene.width = width;
    impl_->scene.height = height;
    return {};
}
Status Viewer::setCamera(const Camera& camera) {
    if (!detail::validGeo(camera.center) || !detail::finite(camera.referenceLatitude) ||
        std::abs(camera.referenceLatitude) > 80 || !detail::finite(camera.pixelsPerDegree) ||
        camera.pixelsPerDegree < .00001 || camera.pixelsPerDegree > 1e7 ||
        !detail::finite(camera.rotationDegrees) ||
        (camera.projection != Projection::Mercator &&
         camera.projection != Projection::LocalEquirectangular) ||
        (camera.projection == Projection::Mercator && std::abs(camera.center.lat) > 85.05112878))
        return detail::invalid("Cámara fuera de los límites geográficos o de escala.");
    auto& s = impl_->scene;
    s.projection = camera.projection;
    s.rotation = std::remainder(camera.rotationDegrees, 360.);
    s.referenceLatitude = camera.referenceLatitude;
    s.centerX = camera.center.lon * detail::lonScale(s);
    s.centerLat = camera.center.lat;
    s.zoom = camera.pixelsPerDegree;
    return {};
}
Camera Viewer::camera() const {
    const auto& s = impl_->scene;
    return {{detail::wrapLongitude(s.centerX / detail::lonScale(s)), s.centerLat},
            s.referenceLatitude,
            s.zoom,
            s.projection,
            s.rotation};
}
bool Viewer::fitCharts() {
    return detail::fitChart(impl_->scene);
}
void Viewer::fitWorld() {
    detail::fitWorld(impl_->scene);
}
Status Viewer::fitBounds(const Bounds& b) {
    if (!b.valid || !detail::validGeo({b.west, b.south}) || !detail::validGeo({b.east, b.north}) ||
        b.south > b.north)
        return detail::invalid("Extensión WGS84 inválida.");
    detail::fitRegion(impl_->scene, b);
    return {};
}

Status Viewer::zoomAt(ScreenPoint cursor, double factor) {
    if (!detail::finite(cursor.x) || !detail::finite(cursor.y) || !detail::finite(factor) ||
        factor <= 0)
        return detail::invalid("Zoom requiere cursor finito y factor positivo.");
    detail::zoomAt(impl_->scene, cursor.x, cursor.y, factor);
    return {};
}
Status Viewer::panBy(double dx, double dy) {
    if (!detail::finite(dx) || !detail::finite(dy))
        return detail::invalid("Desplazamiento no finito.");
    detail::panBy(impl_->scene, dx, dy);
    return {};
}
GeoPoint Viewer::screenToGeo(ScreenPoint point) const {
    const auto p = detail::worldAt(impl_->scene, point.x, point.y);
    return {p.lon, p.lat};
}
ScreenPoint Viewer::geoToScreen(GeoPoint point) const {
    const auto p = detail::toScreen(impl_->scene, {point.lon, point.lat});
    return {p.x, p.y};
}
FeatureInfo Viewer::Impl::describe(std::size_t index) const {
    const auto& f = scene.features[index];
    FeatureInfo result;
    result.id = (static_cast<FeatureId>(generation) << 32) | (index + 1);
    result.layer = f.layer;
    result.objectClass = scene.layers[f.layer].objectClass;
    result.bounds = {detail::wrapLongitude(f.minX), f.minY, detail::wrapLongitude(f.maxX), f.maxY,
                     true};
    for (const auto& a : f.attributes)
        result.attributes.push_back({a.name, a.value});
    result.symbol = describeSymbol(result.objectClass, result.attributes);
    result.description = objectClassDescription(result.objectClass);
    result.decodedAttributes = describeAttributes(result.attributes);
    result.danger = detail::dangerous(scene, f);
    if (!f.points.empty()) {
        result.position = GeoPoint{detail::wrapLongitude(f.points.front().x), f.points.front().y};
        if (std::isfinite(f.points.front().depth))
            result.depth = f.points.front().depth;
    }
    return result;
}
std::vector<FeatureInfo> Viewer::queryFeatures(const Bounds& region, const std::string& objectClass,
                                               std::size_t limit) const {
    std::vector<FeatureInfo> result;
    const auto& scene = impl_->scene;
    for (auto i : scene.spatial.query(region)) {
        if (result.size() >= limit)
            break;
        const auto& f = scene.features[i];
        if (f.points.empty() && f.lines.empty() && f.polygons.empty())
            continue;
        if (!objectClass.empty() && scene.layers[f.layer].objectClass != objectClass)
            continue;
        if (region.valid && !Bounds{f.minX, f.minY, f.maxX, f.maxY, true}.intersects(region))
            continue;
        result.push_back(impl_->describe(i));
    }
    return result;
}
std::optional<FeatureInfo> Viewer::pick(ScreenPoint point) const {
    const auto& s = impl_->scene;
    if (!detail::finite(point.x) || !detail::finite(point.y) || point.x < 0 || point.y < 0 ||
        point.x >= s.width || point.y >= s.height)
        return {};
    const int index = detail::hitTest(s, point.x, point.y);
    if (index < 0)
        return {};
    return impl_->describe(static_cast<std::size_t>(index));
}
std::vector<FeatureInfo> Viewer::pickAll(ScreenPoint point, double tolerance,
                                         std::size_t limit) const {
    std::vector<FeatureInfo> out;
    const auto& s = impl_->scene;
    if (!detail::finite(point.x) || !detail::finite(point.y) || !detail::finite(tolerance) ||
        tolerance < 0 || tolerance > 100 || point.x < 0 || point.y < 0 || point.x >= s.width ||
        point.y >= s.height)
        return out;
    for (auto i : detail::hitTests(s, point.x, point.y, tolerance)) {
        if (out.size() >= limit)
            break;
        out.push_back(impl_->describe(i));
    }
    return out;
}
std::optional<FeatureInfo> Viewer::feature(FeatureId id) const {
    const auto index = static_cast<std::uint32_t>(id);
    if ((id >> 32) != impl_->generation || index == 0 || index > impl_->scene.features.size())
        return {};
    return impl_->describe(index - 1);
}
Status Viewer::select(std::optional<FeatureId> id) {
    if (!id) {
        impl_->scene.selected = -1;
        return {};
    }
    const auto f = feature(*id);
    if (!f)
        return {ErrorCode::NotFound, "Objeto inexistente o de una carga anterior.", {}};
    if (!impl_->scene.showCharts || !impl_->scene.layers[f->layer].visible)
        return detail::invalid("El objeto pertenece a una capa oculta.");
    impl_->scene.selected = static_cast<int>(static_cast<std::uint32_t>(*id) - 1);
    return {};
}
std::optional<FeatureInfo> Viewer::selected() const {
    if (impl_->scene.selected < 0)
        return {};
    return impl_->describe(static_cast<std::size_t>(impl_->scene.selected));
}
} // namespace enc
