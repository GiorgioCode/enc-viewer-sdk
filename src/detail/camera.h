// camera.h — Proyección local y navegación del mapa.
// Convierte geografía y pantalla, encuadra datos y aplica zoom al cursor, desplazamiento e HiDPI.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "app_state.h"
#include <utility>

namespace enc::detail {

// Longitud normalizada y envolvente que admite cruce del antimeridiano.
double wrapLongitude(double longitude);
Bounds viewportBounds(const SceneState& app, double margin = 0);
bool fitRegion(SceneState& app, const Bounds& bounds);
double metresPerPixel(const SceneState& app);

// Relaciona los píxeles físicos del framebuffer con las coordenadas lógicas del mouse.
std::pair<double, double> framebufferScale(const SceneState& app);

// Encuadra todas las celdas con un margen y fija la latitud de referencia de la proyección.
bool fitChart(SceneState& app);

// Encuadra el mundo completo sin modificar las cartas cargadas.
void fitWorld(SceneState& app);

// Corrige el ancho de los grados de longitud según la latitud de referencia.
double lonScale(const SceneState& app);

// Proyecta longitud/latitud a la vista local, con el eje vertical orientado hacia abajo.
Point toScreen(const SceneState& app, Point p);

// Mantiene una geometría continua: p.x y anclaje deben estar desenvueltos en la misma rama.
// worldCopy desplaza una vuelta de longitud sin plegar los vértices de un polígono mundial.
Point toScreenContinuous(const SceneState& app, Point p, double anchorLongitude, int worldCopy = 0);

// Invierte la cámara para convertir una posición del mouse en coordenadas WGS84.
overlays::GeoPoint worldAt(const SceneState& app, double x, double y);

// Encuadra la huella visible del TIFF, incluidas sus correcciones manuales.
void fitRaster(SceneState& app, const overlays::Raster& raster);

// Descarta geometrías fuera de pantalla, reservando un margen para símbolos y etiquetas.
bool inView(const SceneState& app, const Feature& feature, double margin = 35);

// Modifica el zoom dejando inmóvil el punto geográfico que está bajo el cursor.
void zoomAt(SceneState& app, double x, double y, double factor);

// Traslada el centro de la cámara con el desplazamiento lógico del mouse.
void panBy(SceneState& app, double dx, double dy);

} // namespace enc::detail
