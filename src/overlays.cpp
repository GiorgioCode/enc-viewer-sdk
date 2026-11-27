// overlays.cpp — Operaciones públicas de capas GeoTIFF y radar.
// Valida parámetros en CPU, conserva identificadores de imagen y separa simulación de ecos externos.
#include "detail/camera.h"
#include "detail/gdal_support.h"
#include "detail/radar_scene.h"
#include "viewer_impl.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

namespace enc {
namespace {
Status missingRaster() {
    return {ErrorCode::NotFound, "Imagen superpuesta inexistente.", {}};
}
bool unit(double x) {
    return detail::finite(x) && x >= 0 && x <= 1;
}
std::array<detail::overlays::GeoPoint, 4> fallback(const detail::SceneState& s) {
    const auto a = detail::worldAt(s, s.width * .25, s.height * .25),
               b = detail::worldAt(s, s.width * .75, s.height * .75);
    return {{{a.lon, a.lat}, {b.lon, a.lat}, {b.lon, b.lat}, {a.lon, b.lat}}};
}
} // namespace
detail::RasterLayer* Viewer::Impl::raster(OverlayId id) {
    auto& rasters = scene.rasters;
    const auto it = std::find_if(rasters.begin(), rasters.end(),
                                 [id](const auto& layer) { return layer.id == id; });
    return it == rasters.end() ? nullptr : &*it;
}
Result<OverlayId> Viewer::addGeoTiff(const std::filesystem::path& path,
                                     const RasterOptions& options) {
    if (!impl_->startup)
        return {0, impl_->startup};
    if (!unit(options.opacity))
        return {0, detail::invalid("Opacidad TIFF fuera de [0,1].")};
    auto extent = fallback(impl_->scene);
    if (options.manualExtent)
        for (std::size_t i = 0; i < 4; ++i) {
            const auto p = (*options.manualExtent)[i];
            if (!detail::validGeo(p))
                return {0, detail::invalid("Huella manual fuera de WGS84.")};
            extent[i] = {p.lon, p.lat};
        }
    detail::overlays::Raster image;
    std::string error;
    detail::GdalMessages messages(path);
    if (!detail::overlays::loadRaster(image, path, extent, error))
        return {0, {ErrorCode::Decode, error, messages.messages}};
    image.opacity = options.opacity;
    image.visible = options.visible;
    const auto id = impl_->nextOverlay++;
    impl_->scene.rasters.push_back({id, std::move(image)});
    return {id, {ErrorCode::None, "Imagen incorporada.", messages.messages}};
}
std::vector<RasterInfo> Viewer::rasters() const {
    std::vector<RasterInfo> result;
    for (const auto& layer : impl_->scene.rasters) {
        const auto& r = layer.image;
        RasterInfo info{layer.id,  r.path,  r.width,     r.height,    r.visible, r.georeferenced,
                        r.opacity, r.scale, r.offsetLon, r.offsetLat, {}};
        for (std::size_t i = 0; i < 4; ++i) {
            const auto p = r.placedCorner(i);
            info.corners[i] = {p.lon, p.lat};
        }
        result.push_back(std::move(info));
    }
    return result;
}
Status Viewer::setRasterVisible(OverlayId id, bool visible) {
    auto* r = impl_->raster(id);
    if (!r)
        return missingRaster();
    r->image.visible = visible;
    return {};
}
Status Viewer::setRasterOpacity(OverlayId id, double opacity) {
    if (!unit(opacity))
        return detail::invalid("Opacidad fuera de [0,1].");
    auto* r = impl_->raster(id);
    if (!r)
        return missingRaster();
    r->image.opacity = opacity;
    return {};
}
Status Viewer::moveRaster(OverlayId id, double deltaLon, double deltaLat) {
    if (!detail::finite(deltaLon) || !detail::finite(deltaLat))
        return detail::invalid("Desplazamiento TIFF no finito.");
    auto* r = impl_->raster(id);
    if (!r)
        return missingRaster();
    // Rechaza la operación entera si alguna esquina saldría del dominio geográfico admitido.
    for (std::size_t i = 0; i < 4; ++i) {
        const auto p = r->image.placedCorner(i);
        if (!detail::validGeo({p.lon + deltaLon, p.lat + deltaLat}))
            return detail::invalid("El TIFF saldría de los límites WGS84.");
    }
    r->image.offsetLon += deltaLon;
    r->image.offsetLat += deltaLat;
    return {};
}
Status Viewer::scaleRasterAt(OverlayId id, GeoPoint anchor, double factor) {
    if (!detail::validGeo(anchor) || !detail::finite(factor) || factor <= 0)
        return detail::invalid("Escala TIFF requiere anclaje válido y factor positivo.");
    auto* r = impl_->raster(id);
    if (!r)
        return missingRaster();
    auto candidate = r->image;
    candidate.scaleAt({anchor.lon, anchor.lat}, factor);
    for (std::size_t i = 0; i < 4; ++i) {
        const auto p = candidate.placedCorner(i);
        if (!detail::validGeo({p.lon, p.lat}))
            return detail::invalid("El TIFF escalado cruzaría un polo o el antimeridiano.");
    }
    r->image = std::move(candidate);
    return {};
}
Status Viewer::resetRaster(OverlayId id) {
    auto* r = impl_->raster(id);
    if (!r)
        return missingRaster();
    r->image.resetPlacement();
    return {};
}
Status Viewer::fitRaster(OverlayId id) {
    auto* r = impl_->raster(id);
    if (!r)
        return missingRaster();
    detail::fitRaster(impl_->scene, r->image);
    return {};
}
Status Viewer::removeRaster(OverlayId id) {
    if (!impl_->raster(id))
        return missingRaster();
    auto& list = impl_->scene.rasters;
    list.erase(std::remove_if(list.begin(), list.end(),
                              [id](const auto& layer) { return layer.id == id; }),
               list.end());
    return {};
}
Status Viewer::moveRasterToTop(OverlayId id) {
    auto& list = impl_->scene.rasters;
    const auto it =
        std::find_if(list.begin(), list.end(), [id](const auto& layer) { return layer.id == id; });
    if (it == list.end())
        return missingRaster();
    std::rotate(it, it + 1, list.end());
    return {};
}
Result<OverlayId> Viewer::addDemoGeoTiff() {
    if (!impl_->startup)
        return {0, impl_->startup};
    static std::atomic<unsigned> sequence{0};
    std::error_code fsError;
    const auto directory = std::filesystem::temp_directory_path(fsError);
    if (fsError)
        return {0, {ErrorCode::Io, "No se pudo resolver el directorio temporal.", {}}};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = directory / ("enc-sdk-demo-" + std::to_string(stamp) + "-" +
                                   std::to_string(sequence++) + ".tif");
    std::string error;
    if (!detail::overlays::createDemoTiff(path, fallback(impl_->scene), error))
        return {0, {ErrorCode::Io, error, {}}};
    auto result = addGeoTiff(path);
    std::filesystem::remove(
        path, fsError); // La imagen ya está en CPU; no deja un archivo temporal por instancia.
    return result;
}
void Viewer::enableRadar(bool visible) {
    auto& radar = impl_->scene.radar;
    if (visible && !radar.initialized)
        detail::initializeRadar(impl_->scene);
    radar.visible = visible;
}
Status Viewer::configureRadar(const RadarSettings& settings) {
    if (!detail::validGeo(settings.origin) || !detail::finite(settings.rangeNm) ||
        settings.rangeNm < .125 || settings.rangeNm > 48 || !unit(settings.opacity) ||
        !detail::finite(settings.gain) || settings.gain < 0 || settings.gain > 10 ||
        !detail::finite(settings.rpm) || settings.rpm < 1 || settings.rpm > 120)
        return detail::invalid("Radar requiere origen WGS84, alcance 0.125..48 NM, alfa 0..1, "
                               "ganancia 0..10 y RPM 1..120.");
    auto& r = impl_->scene.radar;
    const bool external = settings.mode == RadarMode::ExternalEchoes;
    const bool rebuild = r.external != external || r.origin.lon != settings.origin.lon ||
                         r.origin.lat != settings.origin.lat || r.rangeNm != settings.rangeNm;
    r.visible = settings.visible;
    r.paused = settings.paused;
    r.external = external;
    r.origin = {settings.origin.lon, settings.origin.lat};
    r.rangeNm = settings.rangeNm;
    r.opacity = settings.opacity;
    r.gain = settings.gain;
    r.rpm = settings.rpm;
    r.initialized = true;
    if (!external)
        detail::buildRadarScene(impl_->scene);
    else if (rebuild)
        r.clearScene();
    return {};
}
Status Viewer::setRadarOrigin(GeoPoint origin) {
    auto settings = radarInfo().settings;
    settings.origin = origin;
    return configureRadar(settings);
}
Status Viewer::changeRadarRange(int direction) {
    if (direction != -1 && direction != 1)
        return detail::invalid("El paso de alcance debe ser -1 o +1.");
    auto& r = impl_->scene.radar;
    r.changeRange(direction);
    if (!r.external)
        detail::buildRadarScene(impl_->scene);
    else
        r.clearScene();
    return {};
}
Status Viewer::setRadarEchoes(const std::vector<RadarEcho>& echoes) {
    auto& r = impl_->scene.radar;
    if (!r.external || !r.initialized)
        return detail::invalid("Configurar primero el modo ExternalEchoes.");
    if (echoes.size() > 40000)
        return detail::invalid("Máximo 40000 ecos por escena.");
    for (const auto& e : echoes)
        if (!detail::validGeo(e.position) || !unit(e.intensity))
            return detail::invalid("Eco con posición o intensidad inválida.");
    r.clearScene();
    for (const auto& e : echoes)
        r.addEcho(r.local({e.position.lon, e.position.lat}), e.intensity);
    return {};
}
RadarInfo Viewer::radarInfo() const {
    const auto& r = impl_->scene.radar;
    return {{r.visible,
             r.paused,
             r.external ? RadarMode::ExternalEchoes : RadarMode::Simulation,
             {r.origin.lon, r.origin.lat},
             r.rangeNm,
             r.opacity,
             r.gain,
             r.rpm},
            r.seconds,
            r.sweep(),
            r.coast.size()};
}
Status Viewer::advance(double seconds) {
    if (!detail::finite(seconds) || seconds < 0)
        return detail::invalid("El tiempo transcurrido debe ser finito y no negativo.");
    if (auto status = advanceContacts(seconds); !status)
        return status;
    impl_->scene.radar.advance(seconds);
    return {};
}
} // namespace enc
