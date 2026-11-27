// chart_types.h — Modelo de datos ENC independiente de los objetos GDAL.
// Define geometrías copiadas, atributos, capas, límites y asociaciones de equipos náuticos.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "nautical_symbols.h"
#include <limits>
#include <string>
#include <vector>

namespace enc::detail {

// Modelo propio: la geometría se copia desde GDAL y conserva sus atributos originales.
// En geometrías x/y son longitud/latitud; toScreen devuelve el mismo tipo en píxeles lógicos.
// La profundidad proviene de Z en SOUNDG; NaN significa que no está disponible.
struct Point {
    double x = 0.0;
    double y = 0.0;
    double depth = std::numeric_limits<double>::quiet_NaN();
};

// El primer anillo es exterior; los restantes son huecos. Los triángulos se calculan al cargar.
struct Polygon {
    std::vector<std::vector<Point>> rings;
    std::vector<Point> triangles;
};

// Copia textual del atributo original para inspección y decodificación de simbología.
struct Attribute {
    std::string name;
    std::string value;
};

// Agrupa objetos por celda y clase S-57; visible guarda el interruptor individual del usuario.
struct Layer {
    std::string name;
    std::size_t count = 0;
    bool visible = true;
    std::string objectClass;
    std::string cellKey; // Ruta completa: evita asociar equipos de celdas homónimas.
};

struct Feature {
    std::size_t layer = 0, cell = 0;
    std::vector<Point> points;
    std::vector<std::vector<Point>> lines;
    std::vector<Polygon> polygons;
    std::vector<Attribute> attributes;
    nautical::Symbol symbol;
    // Índices de equipos asociados; -1 indica ausencia. parent evita dibujar un equipo dos veces.
    int topmark = -1, light = -1, parent = -1;
    // Envolvente geográfica para descarte rápido; minScale conserva SCAMIN de S-57.
    double minX = 0, minY = 0, maxX = 0, maxY = 0, minScale = 0, maxScale = 0;
};

} // namespace enc::detail
