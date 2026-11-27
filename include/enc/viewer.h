// viewer.h — Fachada pública del visualizador ENC integrable en una aplicación anfitriona.
// Gestiona datos, cámara, presentación, selección y capas; el anfitrión posee ventana, contexto y reloj.
#pragma once
#include "catalog.h"
#include "symbols.h"
#include <memory>
#include <optional>

namespace enc {
struct LoadOptions {
    bool fitView = true;
    // Por defecto, una celda ilegible impide sustituir la escena anterior.
    bool allowPartial = false;
    ProgressCallback progress;
};
struct LayerInfo {
    std::size_t id = 0, featureCount = 0;
    std::string name, objectClass;
    std::filesystem::path cell;
    bool visible = true;
};
struct FeatureInfo {
    FeatureId id = 0;
    std::size_t layer = 0;
    std::string objectClass;
    Bounds bounds;
    std::vector<Attribute> attributes;
    SymbolDescription symbol;
    std::optional<GeoPoint> position; // Primer punto de anclaje, cuando el objeto lo contiene.
    std::optional<double> depth;      // Z del primer punto, cuando existe; sondas en metros.
};
struct SceneInfo {
    std::size_t cellCount = 0, layerCount = 0, featureCount = 0, triangleCount = 0;
    Bounds bounds;
};

struct RasterOptions {
    double opacity = .55;
    bool visible = true;
    // Sin georreferencia se usa esta huella; si no existe, una región de la vista actual.
    std::optional<std::array<GeoPoint, 4>> manualExtent;
};
struct RasterInfo {
    OverlayId id = 0;
    std::filesystem::path path;
    int width = 0, height = 0;
    bool visible = true, georeferenced = false;
    double opacity = .55, scale = 1, offsetLon = 0, offsetLat = 0;
    std::array<GeoPoint, 4> corners{};
};
enum class RadarMode { Simulation, ExternalEchoes };
struct RadarSettings {
    bool visible = false, paused = false;
    RadarMode mode = RadarMode::Simulation;
    GeoPoint origin;
    double rangeNm = 3, opacity = .6, gain = 1, rpm = 24;
};
// Los ecos externos ya están georreferenciados; la adquisición y el transporte pertenecen al anfitrión.
struct RadarEcho {
    GeoPoint position;
    float intensity = 1;
};
struct RadarInfo {
    RadarSettings settings;
    double seconds = 0, sweepRadians = 0;
    std::size_t echoCount = 0;
};

// Una instancia representa una escena. Serializar sus llamadas; cada instancia GPU usa un contexto.
// Su destructor nunca llama OpenGL: llamar releaseGraphicsResources() antes de destruir el contexto.
class Viewer {
  public:
    Viewer();
    ~Viewer();
    Viewer(const Viewer&) = delete;
    Viewer& operator=(const Viewer&) = delete;
    Viewer(Viewer&&) noexcept;
    Viewer& operator=(Viewer&&) noexcept;

    // Carga síncrona y transaccional en CPU; no necesita ventana ni contexto OpenGL.
    Status loadFolder(const std::filesystem::path& source, const LoadOptions& options = {});
    Status loadCatalog(const Catalog& catalog, const LoadOptions& options = {});
    Status loadCells(const std::vector<std::filesystem::path>& cells,
                     const LoadOptions& options = {});
    void clearCharts();
    SceneInfo sceneInfo() const;
    std::vector<LayerInfo> layers() const;
    Status setLayerVisible(std::size_t layer, bool visible);
    void setChartsVisible(bool visible);
    bool chartsVisible() const;
    void setTheme(Theme theme);
    Theme theme() const;
    Status setPresentation(const Presentation& presentation);
    Presentation presentation() const;

    // Tamaño lógico del mapa, sin paneles del anfitrión. resize(0,0) permite suspender el dibujo.
    Status resize(int width, int height);
    Status setCamera(const Camera& camera);
    Camera camera() const;
    bool fitCharts();
    void fitWorld();
    Status fitBounds(const Bounds& bounds);
    Status zoomAt(ScreenPoint cursor, double factor);
    Status panBy(double dx, double dy);
    GeoPoint screenToGeo(ScreenPoint point) const;
    ScreenPoint geoToScreen(GeoPoint point) const;

    // Los FeatureId incluyen la generación de carga y dejan de ser válidos al sustituir las ENC.
    // Consulta objetos espaciales, incluso de capas ocultas. Bounds inválido equivale a toda la escena.
    std::vector<FeatureInfo> queryFeatures(const Bounds& region = {},
                                           const std::string& objectClass = {},
                                           std::size_t limit = 1000) const;
    std::optional<FeatureInfo> pick(ScreenPoint point) const;
    std::optional<FeatureInfo> feature(FeatureId id) const;
    Status select(std::optional<FeatureId> id);
    std::optional<FeatureInfo> selected() const;

    // Orden de composición: mundo, ENC, TIFF en orden de inserción, radar.
    Result<OverlayId> addGeoTiff(const std::filesystem::path& path,
                                 const RasterOptions& options = {});
    std::vector<RasterInfo> rasters() const;
    Status setRasterVisible(OverlayId id, bool visible);
    Status setRasterOpacity(OverlayId id, double opacity);
    Status moveRaster(OverlayId id, double deltaLon, double deltaLat);
    Status scaleRasterAt(OverlayId id, GeoPoint anchor, double factor);
    Status resetRaster(OverlayId id);
    Status fitRaster(OverlayId id);
    Status removeRaster(OverlayId id);
    Status moveRasterToTop(OverlayId id);
    Result<OverlayId> addDemoGeoTiff();

    void enableRadar(bool visible);
    Status configureRadar(const RadarSettings& settings);
    Status setRadarOrigin(GeoPoint origin);
    Status changeRadarRange(int direction);
    Status setRadarEchoes(const std::vector<RadarEcho>& echoes);
    RadarInfo radarInfo() const;
    // Sólo el anfitrión avanza la simulación; render() no modifica el reloj.
    Status advance(double seconds);

    // Dibuja en el framebuffer que ya esté enlazado, conservando el estado OpenGL del anfitrión.
    // Requiere OpenGL de escritorio >=2.1 de compatibilidad. No crea ventana ni intercambia buffers.
    Status render(const RenderTarget& target);
    Status renderSymbolCatalogue(const RenderTarget& target);
    // Requiere el mismo contexto vigente. Conserva datos CPU para volver a subir las texturas.
    Status releaseGraphicsResources();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace enc
