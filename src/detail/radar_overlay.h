// radar_overlay.h — Modelo de simulación de radar PPI, independiente de OpenGL.
// Gestiona alcance, reloj, persistencia, retornos y blancos en coordenadas relativas al origen.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "geo_types.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace enc::detail::overlays {
constexpr double pi = 3.14159265358979323846;
// Alcances discretos expresados en millas náuticas.
constexpr std::array<double, 10> radarRanges = {.125, .25, .5, .75, 1.5, 3, 6, 12, 24, 48};
// Coordenadas relativas al origen del PPI, en millas náuticas hacia este y norte.
struct LocalPoint {
    double east = 0, north = 0;
};
// Un retorno combina ubicación relativa e intensidad normalizada.
struct Echo {
    LocalPoint point;
    float intensity = 1;
};
// La simulación no depende de OpenGL: el renderizador consulta este estado sin avanzar el tiempo.
struct Radar {
    bool visible = false, paused = false, initialized = false, placingOrigin = false;
    bool external = false; // El anfitrión puede sustituir la simulación por retornos geográficos.
    GeoPoint origin;
    // Alcance en NM, velocidad en revoluciones por minuto y reloj simulado en segundos.
    double rangeNm = 3, opacity = .6, gain = 1, rpm = 24, seconds = 0;
    std::vector<Echo> coast;
    std::array<LocalPoint, 3> targets{};
    // Índices espaciales ocupados para deduplicar muestras de costa sin búsquedas cuadráticas.
    std::unordered_set<std::uint64_t> echoBins;

    // Protege la conversión de longitud cerca de los polos.
    double cosine() const;

    // Expresa una posición geográfica como desplazamiento este/norte en millas náuticas.
    LocalPoint local(GeoPoint p) const;

    // Convierte un eco local a WGS84 para que siga la cámara de la carta.
    GeoPoint geographic(LocalPoint p) const;

    // Obtiene el ángulo del barrido a partir del tiempo simulado y las revoluciones por minuto.
    double sweep() const;

    // Avanza sólo cuando el radar está visible y en marcha, limitando los saltos de tiempo.
    void advance(double dt);

    // Recorre los alcances normalizados sin salir de los extremos disponibles.
    void changeRange(int direction);

    // Atenúa el fósforo según el tiempo transcurrido desde el último paso del barrido.
    double persistence(LocalPoint p) const;

    // Descarta los ecos y sus celdas ocupadas antes de reconstruir una escena.
    void clearScene();

    // Deduplica retornos próximos y limita el número de muestras a dibujar.
    void addEcho(LocalPoint p, float intensity = 1);

    // Recorta un segmento contra el círculo de alcance y lo muestrea con densidad uniforme.
    void addSegment(GeoPoint start, GeoPoint finish, float intensity = 1);

    // Desplaza un blanco sintético alrededor de su posición inicial de manera determinista.
    LocalPoint targetAt(std::size_t index, double time) const;
};
} // namespace enc::detail::overlays
