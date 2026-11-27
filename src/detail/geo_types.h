// geo_types.h — Coordenadas geográficas comunes a las superposiciones.
// Define posiciones WGS84 para que el radar y los TIFF compartan un tipo sin depender entre sí.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

namespace enc::detail::overlays {
// Longitud y latitud WGS84, expresadas en grados y en orden geográfico tradicional.
struct GeoPoint {
    double lon = 0, lat = 0;
};
} // namespace enc::detail::overlays
