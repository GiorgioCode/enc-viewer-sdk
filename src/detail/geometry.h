// geometry.h — Conversión y operaciones geométricas del visor.
// Copia geometrías OGR, tesela superficies con huecos y ofrece consultas para selección y límites.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "app_state.h"

class OGRGeometry;

namespace enc::detail {

// Amplía la extensión de las cartas con coordenadas válidas; el mapa mundial no participa.
void extendBounds(SceneState& app, double lon, double lat);

// Tesela contornos y huecos una sola vez durante la carga, sin rellenar los huecos.
void triangulate(Polygon& polygon);

// Recorre puntos, líneas, polígonos y colecciones OGR, conservando las profundidades Z.
void appendGeometry(OGRGeometry* geometry, Feature& feature, SceneState& app);

// Evalúa si un punto está dentro de un anillo con el algoritmo de cruce de rayos.
bool inRing(Point p, const std::vector<Point>& ring);

// Calcula la distancia de un punto a un segmento, incluidos los segmentos degenerados.
double segmentDistance(Point p, Point a, Point b);

} // namespace enc::detail
