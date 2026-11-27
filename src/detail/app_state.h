// app_state.h — Estado privado de una escena, sin ventanas ni controles de una aplicación concreta.
// Conserva geometrías, cámara, presentación y capas; la API pública lo oculta mediante PIMPL.
#pragma once
#include "chart_types.h"
#include "contact_state.h"
#include "radar_overlay.h"
#include "raster_overlay.h"
#include "spatial_index.h"
#include <enc/portrayal.h>
#include <enc/types.h>
#include <filesystem>
#include <unordered_map>
namespace enc::detail {
namespace fs = std::filesystem;
// El mapa ocupa toda la región entregada por el anfitrión; no reserva paneles propios.
constexpr int kSidebarWidth = 0, kToolbarHeight = 0, kStatusHeight = 0;
constexpr double kPi = 3.14159265358979323846;
struct RasterLayer {
    enc::OverlayId id = 0;
    overlays::Raster image;
};
struct CellData {
    fs::path path;
    int compilationScale = 0, intendedUsage = 0;
    std::vector<Polygon> coverage;
    bool hasCoverage = false;
    Bounds bounds;
};
struct MosaicCache;
struct SceneState {
    int width = 1280, height = 800, framebufferWidth = 1280, framebufferHeight = 800;
    int viewportX = 0, viewportY = 0;
    std::vector<Layer> layers;
    std::vector<Feature> features, baseLand;
    std::vector<fs::path> indexedCells;
    std::vector<CellData> cells;
    mutable std::shared_ptr<MosaicCache> mosaicCache; // Se invalida al sustituir celdas o cámara.
    SpatialIndex spatial;
    std::vector<enc::PortrayalRule> rules = enc::defaultPortrayalRules();
    int renderingCell = -1; // Sólo filtra durante el pintado de una celda del mosaico.
    bool showCharts = true, nightMode = false, hasBounds = false;
    enc::Presentation presentation;
    std::vector<RasterLayer> rasters;
    overlays::Raster fontAtlas;
    std::unordered_map<std::uint64_t, int> labelPositions;
    std::shared_ptr<overlays::GpuResources> gpu = std::make_shared<overlays::GpuResources>();
    overlays::Radar radar;
    ContactsState contacts;
    double centerX = 0, centerLat = 0, referenceLatitude = 0, zoom = 100;
    enc::Projection projection = enc::Projection::LocalEquirectangular;
    double rotation = 0;
    double minLon = 0, maxLon = 0, minLat = 0, maxLat = 0;
    int selected = -1;
};
} // namespace enc::detail
