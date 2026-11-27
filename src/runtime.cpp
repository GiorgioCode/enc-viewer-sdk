// runtime.cpp — Inicialización compartida y utilidades de los tipos públicos.
// Registra GDAL una sola vez sin alterar opciones globales ni destruir controladores del anfitrión.
#include <enc/types.h>
#include <enc/version.h>
#include <gdal_priv.h>
#include <mutex>
#include <ogr_geometry.h>

namespace enc {
Status initialize() {
    static std::once_flag registration;
    std::call_once(registration, [] { GDALAllRegister(); });
    if (!GetGDALDriverManager()->GetDriverByName("S57"))
        return {ErrorCode::Unsupported, "GDAL no incluye el controlador S57.", {}};
    if (!OGRGeometryFactory::haveGEOS())
        return {ErrorCode::Unsupported,
                "GDAL requiere GEOS para recortar la cobertura del mosaico.",
                {}};
    return {};
}
const char* version() noexcept {
    return ENC_VERSION_STRING;
}
bool Bounds::intersects(const Bounds& other) const {
    if (!valid || !other.valid || south > other.north || north < other.south)
        return false;
    const double aEnd = east < west ? east + 360 : east;
    const double bEnd = other.east < other.west ? other.east + 360 : other.east;
    for (int shift = -1; shift <= 1; ++shift)
        if (west <= bEnd + shift * 360 && aEnd >= other.west + shift * 360)
            return true;
    return false;
}
} // namespace enc
