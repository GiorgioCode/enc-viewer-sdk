// core_test.cpp — Verificación de la API pública de indexación y escena sin contexto gráfico.
// Comprueba transacciones, cancelación, consultas, identificadores, simbología y validación de parámetros.
#include <algorithm>
#include <cmath>
#include <enc/enc.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const std::string& message) {
    if (!value)
        throw std::runtime_error(message);
}
void ok(const enc::Status& status) {
    require(static_cast<bool>(status), status.message);
}
std::string attribute(const enc::FeatureInfo& f, const std::string& name) {
    for (const auto& a : f.attributes)
        if (a.name == name)
            return a.value;
    return {};
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Se requieren carpeta de cartas y carpeta temporal.");
        const std::filesystem::path charts = argv[1], temp = argv[2];
        std::filesystem::create_directories(temp);
        ok(enc::initialize());
        require(std::string(enc::version()) == ENC_VERSION_STRING, "Versión incorrecta.");
        enc::Catalog catalog;
        ok(catalog.scan(charts));
        require(catalog.cells().size() == 3, "Indexación recursiva incorrecta.");
        std::size_t updates = 0;
        for (const auto& cell : catalog.cells()) {
            require(cell.readable && cell.bounds.valid && cell.compilationScale == 12000,
                    "Metadatos DSID/cobertura incompletos.");
            updates += cell.updates.size();
        }
        require(updates == 12, "Las actualizaciones .001..007 no se asociaron a sus bases.");
        require(catalog.query({-72, 42, -70, 43, true}).size() == 3 &&
                    catalog.query({0, 0, 1, 1, true}).empty(),
                "Consulta espacial de celdas incorrecta.");
        const auto originalSource = catalog.source();
        enc::ScanOptions cancelledScan;
        cancelledScan.progress = [](const auto&) { return false; };
        require(catalog.scan(charts, cancelledScan).code == enc::ErrorCode::Cancelled &&
                    catalog.cells().size() == 3 && catalog.source() == originalSource,
                "Cancelar descartó el índice anterior.");
        require(!catalog.scan(temp / "inexistente") && catalog.cells().size() == 3,
                "Una ruta inválida descartó el índice.");
        enc::ScanOptions shallow;
        shallow.recursive = false;
        enc::Catalog flat;
        ok(flat.scan(charts, shallow));
        require(flat.cells().empty(), "El recorrido no recursivo entró en subcarpetas.");
        ok(catalog.saveInventory(temp / "inventory.json"));
        std::ifstream json(temp / "inventory.json");
        const std::string contents((std::istreambuf_iterator<char>(json)), {});
        require(contents.find("enc-sdk-inventory-1") != std::string::npos &&
                    contents.find("US5BOSCC") != std::string::npos,
                "Inventario JSON incompleto.");
        ok(catalog.refresh());

        enc::Viewer viewer;
        ok(viewer.loadCatalog(catalog));
        const auto initial = viewer.sceneInfo();
        require(initial.cellCount == 3 && initial.featureCount > 6000 &&
                    initial.triangleCount > 150000,
                "Faltan geometrías ENC o teselación.");
        require(viewer.render({0, 0, 800, 600}).code == enc::ErrorCode::Graphics,
                "render sin contexto no devolvió error explícito.");
        ok(viewer.releaseGraphicsResources());
        for (const auto& layer : viewer.layers())
            if (layer.objectClass.rfind("M_", 0) == 0)
                require(!layer.visible, "Metadatos activados por defecto.");
        const auto before = viewer.camera();
        const auto anchor = viewer.screenToGeo({320, 210});
        ok(viewer.zoomAt({320, 210}, 2));
        const auto screen = viewer.geoToScreen(anchor);
        require(std::hypot(screen.x - 320, screen.y - 210) < 1e-6, "Zoom sin anclaje al cursor.");
        ok(viewer.setCamera(before));
        enc::LoadOptions cancelledLoad;
        cancelledLoad.progress = [](const auto& p) { return p.completed == 0; };
        require(viewer.loadCatalog(catalog, cancelledLoad).code == enc::ErrorCode::Cancelled &&
                    viewer.sceneInfo().featureCount == initial.featureCount,
                "Cancelar modificó la escena.");
        require(!viewer.loadFolder(temp / "inexistente") && viewer.sceneInfo().cellCount == 3,
                "Un fallo destruyó las cartas anteriores.");
        require(!viewer.loadCells({catalog.cells()[0].path, temp / "missing.000"}) &&
                    viewer.sceneInfo().cellCount == 3,
                "La carga estricta aceptó una colección parcial.");
        const auto buoys = viewer.queryFeatures({}, "BOYLAT");
        const auto buoy = std::find_if(buoys.begin(), buoys.end(), [](const auto& f) {
            return attribute(f, "OBJNAM") == "UMass Buoy 2";
        });
        require(buoy != buoys.end() && buoy->position && buoy->symbol.shape == 1 &&
                    buoy->symbol.colours == std::vector<unsigned>{0xC43D38},
                "La boya real no se interpreta por atributos.");
        auto close = before;
        close.center = *buoy->position;
        close.pixelsPerDegree = 45000;
        ok(viewer.setCamera(close));
        auto p = viewer.geoToScreen(*buoy->position);
        p.y -= 8;
        const auto picked = viewer.pick(p);
        require(picked && picked->symbol.kind == enc::SymbolKind::Buoy,
                "La selección no prioriza el cuerpo de la boya.");
        ok(viewer.select(picked->id));
        require(viewer.selected().has_value(), "No se conserva la selección.");
        viewer.setTheme(enc::Theme::Night);
        viewer.setChartsVisible(false);
        require(!viewer.pick(p) && !viewer.selected() &&
                    viewer.sceneInfo().featureCount == initial.featureCount,
                "Ocultar ENC no conserva datos o deja objetos seleccionables.");
        viewer.setChartsVisible(true);
        const auto oldId = buoy->id;
        ok(viewer.loadCatalog(catalog));
        require(!viewer.feature(oldId) && viewer.theme() == enc::Theme::Night,
                "La recarga no invalida IDs o pierde el tema.");
        const auto soundings = viewer.queryFeatures({}, "SOUNDG", 10000);
        require(std::any_of(soundings.begin(), soundings.end(),
                            [](const auto& f) { return f.depth && *f.depth != 0; }),
                "Profundidad Z no disponible al integrador.");
        enc::LoadOptions partial;
        partial.allowPartial = true;
        const auto partly =
            viewer.loadCells({catalog.cells()[0].path, temp / "missing.000"}, partial);
        require(partly && viewer.sceneInfo().cellCount == 1 && !partly.diagnostics.empty(),
                "La carga parcial no informa fallos.");
        ok(viewer.loadCells({catalog.cells()[0].path, catalog.cells()[0].path}));
        require(viewer.sceneInfo().cellCount == 1, "La celda duplicada se dibujaría dos veces.");

        require(enc::symbolExamples().size() >= 44, "Catálogo ampliado incompleto.");
        for (int direction = 1; direction <= 4; ++direction) {
            const auto cardinal = enc::describeSymbol(
                "BOYCAR", {{"CATCAM", std::to_string(direction)}, {"BOYSHP", "4"}});
            const int top[] = {13, 11, 14, 10};
            require(cardinal.topShape == top[direction - 1], "Marca cardinal incorrecta.");
        }
        require(!enc::describeSymbol("UNSUPPORTED", {}).recognized,
                "El símbolo desconocido tiene significado inventado.");
        require(enc::describeSymbol("UWTROC", {{"WATLEV", "5"}}).waterLevel == 5,
                "Se perdió WATLEV.");
        const auto presentation = viewer.presentation();
        auto invalid = presentation;
        invalid.safetyDepth = -1;
        require(!viewer.setPresentation(invalid) &&
                    viewer.presentation().safetyDepth == presentation.safetyDepth,
                "La validación modifica el estado.");
        require(!viewer.resize(-1, 400) && !viewer.zoomAt({1, 1}, 0) &&
                    !viewer.panBy(std::numeric_limits<double>::quiet_NaN(), 0),
                "Se aceptaron parámetros de cámara inválidos.");
        require(!viewer.setLayerVisible(999999, true) && !viewer.select(123),
                "Un ID inválido no se diagnostica.");
        const auto demo = viewer.addDemoGeoTiff();
        require(static_cast<bool>(demo), demo.status.message);
        require(viewer.rasters().size() == 1, "No se cargó TIFF en CPU.");
        const auto rasterBefore = viewer.rasters()[0].corners[0];
        ok(viewer.scaleRasterAt(demo.value, rasterBefore, 2));
        const auto rasterAfter = viewer.rasters()[0].corners[0];
        require(std::hypot(rasterAfter.lon - rasterBefore.lon, rasterAfter.lat - rasterBefore.lat) <
                    1e-9,
                "Escalado TIFF sin anclaje.");
        require(!viewer.addGeoTiff(temp / "missing.tif") && viewer.rasters().size() == 1,
                "Un fallo TIFF alteró las imágenes.");
        ok(viewer.removeRaster(demo.value));
        require(viewer.rasters().empty(), "No se eliminó la capa CPU.");
        viewer.clearCharts();
        require(viewer.sceneInfo().cellCount == 0 && !viewer.feature(oldId),
                "clearCharts dejó datos ENC.");
        std::cout << "PASS: API CPU, índice, actualizaciones, JSON, transacciones, cancelación, "
                     "cámara, selección, símbolos y TIFF sin contexto.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
