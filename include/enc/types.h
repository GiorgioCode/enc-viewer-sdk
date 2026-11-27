// types.h — Tipos públicos del SDK ENC, sin cabeceras GDAL, OpenGL ni de interfaz gráfica.
// Define coordenadas, diagnósticos, identificadores y opciones compartidas entre catálogo y visor.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace enc {

// Las coordenadas geográficas siempre usan longitud/latitud WGS84 en grados.
struct GeoPoint {
    double lon = 0, lat = 0;
};
struct ScreenPoint {
    double x = 0, y = 0;
};
struct Bounds {
    double west = 0, south = 0, east = 0, north = 0;
    bool valid = false;
    // west > east representa una región que cruza el antimeridiano.
    bool intersects(const Bounds& other) const;
};

enum class ErrorCode {
    None,
    InvalidArgument,
    Io,
    Unsupported,
    Decode,
    Cancelled,
    Graphics,
    NotFound
};
enum class Severity { Info, Warning, Error };
struct Diagnostic {
    Severity severity = Severity::Info;
    std::filesystem::path path;
    std::string message;
};
struct Status {
    ErrorCode code = ErrorCode::None;
    std::string message;
    std::vector<Diagnostic> diagnostics;
    explicit operator bool() const {
        return code == ErrorCode::None;
    }
};
template <class T> struct Result {
    T value{};
    Status status;
    explicit operator bool() const {
        return static_cast<bool>(status);
    }
};

// El callback devuelve false para cancelar entre celdas; no debe reentrar al mismo objeto.
struct Progress {
    std::size_t completed = 0, total = 0;
    std::filesystem::path path;
};
using ProgressCallback = std::function<bool(const Progress&)>;
using FeatureId = std::uint64_t;
using OverlayId = std::uint64_t;

struct Attribute {
    std::string name, value;
};
enum class Theme { Day, Night };

// Profundidades en metros, símbolos en píxeles lógicos. No es una presentación S-52 certificada.
enum class Projection { LocalEquirectangular, Mercator };
enum class DisplayMode { Base, Standard, All };
struct Presentation {
    double shallowDepth = 2, safetyDepth = 5, deepDepth = 10;
    double symbolScale = 1;
    bool showLabels = true, showSoundings = true, showLightSectors = true;
    double safetyContour =
        5; // Se utiliza el contorno disponible igual o inmediatamente más profundo.
    double textScale = 1, labelDensity = 1, millimetresPerPixel = 0.2645833333;
    bool preferNationalNames = false, respectScale = true, automaticMosaic = true;
    bool showScaleBar = true, showOverscale = true;
    double detailThreshold = 8; // Relación máxima denominador visible / escala de compilación.
    DisplayMode displayMode = DisplayMode::All;
};
struct Camera {
    GeoPoint center;
    double referenceLatitude = 0;
    double pixelsPerDegree = 100;
    Projection projection = Projection::LocalEquirectangular;
    double rotationDegrees = 0; // Giro horario del mapa en pantalla.
};

// Región física del framebuffer ACTUAL. El origen x/y de OpenGL está abajo a la izquierda.
// Las coordenadas de navegación son locales al mapa lógico y tienen origen arriba a la izquierda.
struct RenderTarget {
    int x = 0, y = 0, width = 1280, height = 800;
};

// Registra controladores GDAL una vez por proceso; no destruye el registro global al terminar.
Status initialize();
const char* version() noexcept;
} // namespace enc
