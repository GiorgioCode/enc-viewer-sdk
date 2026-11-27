// runtime.cpp — Inicialización compartida y utilidades de los tipos públicos.
// Registra GDAL una sola vez sin alterar opciones globales ni destruir controladores del anfitrión.
#include <enc/types.h>
#include <enc/version.h>
#include <gdal_priv.h>
#include <mutex>

namespace enc {
Status initialize() {
    static std::once_flag registration;
    std::call_once(registration, [] { GDALAllRegister(); });
    if (!GetGDALDriverManager()->GetDriverByName("S57"))
        return {ErrorCode::Unsupported, "GDAL no incluye el controlador S57.", {}};
    return {};
}
const char* version() noexcept {
    return ENC_VERSION_STRING;
}
bool Bounds::intersects(const Bounds& other) const {
    return valid && other.valid && west <= other.east && east >= other.west &&
           south <= other.north && north >= other.south;
}
} // namespace enc
