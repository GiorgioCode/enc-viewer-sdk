// viewer.h — Fachada pública del visualizador ENC integrable en una aplicación anfitriona.
// Gestiona datos, cámara, presentación, selección y capas; el anfitrión posee ventana, contexto y reloj.
#pragma once
#include "catalog.h"
#include "contacts.h"
#include "portrayal.h"
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
    std::string description;
    std::vector<AttributeDescription> decodedAttributes;
    bool danger = false;
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

struct ScaleInfo {
    double denominator = 0, metresPerPixel = 0, barMetres = 0, barPixels = 0;
    bool overscale = false;
};
struct VisibleCell {
    std::filesystem::path path;
    int compilationScale = 0, intendedUsage = 0;
    double overscaleFactor = 0;
};
struct CellContour {
    std::filesystem::path cell;
    std::optional<double> contour;
};
struct DepthInfo {
    double requestedContour = 0;
    std::optional<double>
        displayedContour; // Valor común cuando coincide en todas las celdas visibles.
    std::vector<CellContour> cells;
};
class ChartStreamer;
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
    Status setPortrayalRules(const std::vector<PortrayalRule>& overrides);
    std::vector<PortrayalRule> portrayalRules() const;
    ScaleInfo scaleInfo() const;
    std::vector<VisibleCell> visibleCells() const;
    DepthInfo depthInfo() const;

    // Tamaño lógico del mapa, sin paneles del anfitrión. resize(0,0) permite suspender el dibujo.
    Status resize(int width, int height);
    Status setCamera(const Camera& camera);
    Camera camera() const;
    bool fitCharts();
    void fitWorld();
    Status fitBounds(const Bounds& bounds);
    Status fitFeatures(const std::vector<FeatureId>& features);
    Bounds viewBounds(double marginPixels = 0) const;
    // Guardar al comenzar un gesto; permite agrupar arrastres y series de zoom en un paso.
    void rememberView();
    bool previousView();
    bool nextView();
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
    std::vector<FeatureInfo> pickAll(ScreenPoint point, double tolerancePixels = 6,
                                     std::size_t limit = 100) const;
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

    // Contactos independientes de las celdas, con IDs estables suministrados por el anfitrión.
    Status configureContacts(const ContactSettings& settings);
    ContactSettings contactSettings() const;
    Status upsertContacts(const std::vector<Contact>& reports);
    Status removeContact(const ContactId& id);
    void clearContacts();
    std::vector<ContactInfo> contacts() const;
    std::optional<ContactInfo> contact(const ContactId& id) const;
    Status selectContact(std::optional<ContactId> id);
    Status acknowledgeContact(const ContactId& id);
    // Selecciona por distancia al punto de referencia del contacto; tolerancia en píxeles lógicos.
    std::vector<ContactInfo> pickContacts(ScreenPoint point, double tolerancePixels = 16) const;
    Status setOwnShip(std::optional<OwnShip> ship);
    std::optional<OwnShip> ownShip() const;
    double contactTime() const;
    Status advanceContacts(double seconds); // Reloj completo; advance() también lo avanza una vez.
    ContactLayerInfo contactLayerInfo() const;

    // Dibuja en el framebuffer que ya esté enlazado, conservando el estado OpenGL del anfitrión.
    // Requiere OpenGL de escritorio >=2.1 de compatibilidad. No crea ventana ni intercambia buffers.
    Status render(const RenderTarget& target);
    Status renderSymbolCatalogue(const RenderTarget& target);
    // Requiere el mismo contexto vigente. Conserva datos CPU para volver a subir las texturas.
    Status releaseGraphicsResources();

  private:
    friend class ChartStreamer;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace enc
