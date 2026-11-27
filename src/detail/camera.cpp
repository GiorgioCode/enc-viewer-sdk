// camera.cpp — Proyección local y navegación del mapa.
// Convierte geografía y pantalla, encuadra datos y aplica zoom al cursor, desplazamiento e HiDPI.
// Contiene la implementación de esta responsabilidad.

#include "camera.h"
#include <algorithm>
#include <cmath>

namespace enc::detail {

// Relaciona los píxeles físicos del framebuffer con las coordenadas lógicas del mouse.
std::pair<double, double> framebufferScale(const SceneState& app) {
    return {static_cast<double>(app.framebufferWidth) / std::max(1, app.width),
            static_cast<double>(app.framebufferHeight) / std::max(1, app.height)};
}

// Encuadra todas las celdas con un margen y fija la latitud de referencia de la proyección.
bool fitChart(SceneState& app) {
    if (!app.hasBounds)
        return false;
    app.referenceLatitude = std::clamp((app.minLat + app.maxLat) * 0.5, -80.0, 80.0);
    // Aproximación equirectangular local: comprime la longitud según la latitud de referencia.
    const double lonScale = std::max(0.17, std::cos(app.referenceLatitude * kPi / 180.0));
    const double width = std::max(1e-8, app.maxLon - app.minLon) * lonScale;
    const double height = std::max(1e-8, app.maxLat - app.minLat);
    app.centerX = (app.minLon + app.maxLon) * 0.5 * lonScale;
    app.centerLat = (app.minLat + app.maxLat) * 0.5;
    // Encuadra sólo el área del mapa, descontando el lateral y las barras.
    app.zoom = std::clamp(std::min((app.width - kSidebarWidth) / width,
                                   (app.height - kToolbarHeight - kStatusHeight) / height) *
                              0.88,
                          0.00001, 10000000.0);
    return true;
}

// Encuadra el mundo completo sin modificar las cartas cargadas.
void fitWorld(SceneState& app) {
    app.referenceLatitude = 0;
    app.centerX = 0;
    app.centerLat = 0;
    app.zoom =
        std::max(0.1, std::min(std::max(1, app.width - kSidebarWidth) / 360.0,
                               std::max(1, app.height - kToolbarHeight - kStatusHeight) / 180.0) *
                          0.94);
}

// Corrige el ancho de los grados de longitud según la latitud de referencia.
double lonScale(const SceneState& app) {
    return std::max(0.17, std::cos(app.referenceLatitude * kPi / 180.0));
}

// Proyecta longitud/latitud a la vista local, con el eje vertical orientado hacia abajo.
Point toScreen(const SceneState& app, Point p) {
    const double mapW = app.width - kSidebarWidth,
                 mapH = app.height - kToolbarHeight - kStatusHeight;
    return {mapW * 0.5 + (p.x * lonScale(app) - app.centerX) * app.zoom,
            kToolbarHeight + mapH * 0.5 + (app.centerLat - p.y) * app.zoom};
}

// Invierte la cámara para convertir una posición del mouse en coordenadas WGS84.
overlays::GeoPoint worldAt(const SceneState& app, double x, double y) {
    return {(app.centerX + (x - (app.width - kSidebarWidth) * .5) / app.zoom) / lonScale(app),
            app.centerLat -
                (y - kToolbarHeight - (app.height - kToolbarHeight - kStatusHeight) * .5) /
                    app.zoom};
}

// Encuadra la huella visible del TIFF, incluidas sus correcciones manuales.
void fitRaster(SceneState& app, const overlays::Raster& raster) {
    if (!raster.loaded())
        return;
    double left = 180, right = -180, bottom = 90, top = -90;
    for (int i = 0; i < 4; ++i) {
        const auto p = raster.placedCorner(i);
        left = std::min(left, p.lon);
        right = std::max(right, p.lon);
        bottom = std::min(bottom, p.lat);
        top = std::max(top, p.lat);
    }
    app.referenceLatitude = std::clamp((bottom + top) * .5, -80.0, 80.0);
    app.centerX = (left + right) * .5 * lonScale(app);
    app.centerLat = (bottom + top) * .5;
    app.zoom = std::clamp(
        std::min((app.width - kSidebarWidth) / std::max(1e-8, (right - left) * lonScale(app)),
                 (app.height - kToolbarHeight - kStatusHeight) / std::max(1e-8, top - bottom)) *
            .82,
        .00001, 10000000.0);
}

// Descarta geometrías fuera de pantalla, reservando un margen para símbolos y etiquetas.
bool inView(const SceneState& app, const Feature& feature, double margin) {
    const Point a = toScreen(app, {feature.minX, feature.maxY});
    const Point b = toScreen(app, {feature.maxX, feature.minY});
    return b.x >= -margin && a.x <= app.width - kSidebarWidth + margin &&
           b.y >= kToolbarHeight - margin && a.y <= app.height - kStatusHeight + margin;
}

// Modifica el zoom dejando inmóvil el punto geográfico que está bajo el cursor.
void zoomAt(SceneState& app, double x, double y, double factor) {
    const double mapW = app.width - kSidebarWidth,
                 mapH = app.height - kToolbarHeight - kStatusHeight;
    const double dx = x - mapW * 0.5, dy = y - kToolbarHeight - mapH * 0.5;
    // Guarda el punto bajo el cursor y recompone el centro tras cambiar la escala.
    const double wx = app.centerX + dx / app.zoom, wy = app.centerLat - dy / app.zoom;
    app.zoom = std::clamp(app.zoom * factor, 0.00001, 10000000.0);
    app.centerX = wx - dx / app.zoom;
    app.centerLat = wy + dy / app.zoom;
}

// Traslada el centro de la cámara con el desplazamiento lógico del mouse.
void panBy(SceneState& app, double dx, double dy) {
    app.centerX -= dx / app.zoom;
    app.centerLat += dy / app.zoom;
}

} // namespace enc::detail
