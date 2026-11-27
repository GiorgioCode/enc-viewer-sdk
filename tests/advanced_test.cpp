// advanced_test.cpp — Contratos de cámara global, presentación, consultas y carga progresiva.
// Usa la API pública y datos incluidos; comprueba confirmación transaccional y reutilización de caché.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <enc/enc.h>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
void require(bool b, const char* message) {
    if (!b)
        throw std::runtime_error(message);
}
void ok(const enc::Status& s) {
    if (!s)
        throw std::runtime_error(s.message);
}
void finish(enc::ChartStreamer& stream, enc::Viewer& viewer) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (stream.info().loading) {
        stream.poll(viewer);
        require(std::chrono::steady_clock::now() < deadline, "Carga progresiva bloqueada.");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ok(stream.info().lastResult);
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Falta la carpeta de cartas.");
        enc::Catalog catalog;
        ok(catalog.scan(argv[1]));
        enc::Viewer v;
        ok(v.resize(900, 700));
        for (auto projection : {enc::Projection::LocalEquirectangular, enc::Projection::Mercator})
            for (double rotation : {0., 45., 90., -130.}) {
                enc::Camera c{{179.9, 60}, 60, 12000, projection, rotation};
                ok(v.setCamera(c));
                for (enc::GeoPoint p :
                     std::vector<enc::GeoPoint>{{-179.99, 60.01}, {179.91, 59.99}}) {
                    const auto a = v.geoToScreen(p);
                    const auto b = v.screenToGeo(a);
                    require(std::abs(std::remainder(b.lon - p.lon, 360.)) < 1e-8 &&
                                std::abs(b.lat - p.lat) < 1e-8,
                            "Proyección/rotación no invertible.");
                }
                const auto p = v.screenToGeo({240, 180});
                ok(v.zoomAt({240, 180}, 1.7));
                const auto screen = v.geoToScreen(p);
                require(std::hypot(screen.x - 240, screen.y - 180) < 1e-6,
                        "Zoom rotado/Mercator perdió el anclaje.");
                const auto centre = v.screenToGeo({450, 350});
                ok(v.panBy(35, -20));
                const auto shifted = v.geoToScreen(centre);
                require(std::hypot(shifted.x - 485, shifted.y - 330) < 1e-6,
                        "Arrastre rotado no sigue el mouse.");
            }
        ok(v.fitBounds({179.8, 59.9, -179.8, 60.1, true}));
        require(std::abs(v.camera().center.lon) > 179,
                "Encuadre del antimeridiano dio la vuelta al mundo.");
        v.rememberView();
        const auto saved = v.camera();
        ok(v.panBy(100, 0));
        require(v.previousView() && std::abs(v.camera().center.lon - saved.center.lon) < 1e-8,
                "Historial no restaura la cámara.");
        require(v.nextView(), "Falta historial hacia adelante.");
        auto p = v.presentation();
        p.millimetresPerPixel = .2;
        ok(v.setPresentation(p));
        const auto scale = v.scaleInfo();
        require(scale.barPixels > 0 && scale.barMetres > 0, "Escala gráfica vacía.");
        p.millimetresPerPixel = .4;
        ok(v.setPresentation(p));
        require(std::abs(v.scaleInfo().denominator * 2 - scale.denominator) < 1e-6,
                "Calibración física ignorada.");
        enc::PortrayalRule rule;
        rule.objectClass = "COALNE";
        rule.line = 0xff00ff;
        rule.lineWidth = 3;
        rule.category = enc::DisplayCategory::Base;
        ok(v.setPortrayalRules({rule}));
        require(!v.setPortrayalRules({rule, rule}), "Reglas duplicadas aceptadas.");
        require(enc::objectClassDescription("BOYLAT") != "BOYLAT" &&
                    enc::attributeDescription("BOYSHP") != "BOYSHP",
                "No se resuelven diccionarios GDAL.");
        const auto decoded = enc::describeAttributes({{"BOYSHP", "1"}});
        require(!decoded[0].meaning.empty(), "No se decodifica un atributo enumerado.");
        enc::ChartStreamer stream(catalog);
        const auto region = enc::Bounds{-72, 42, -70, 43, true};
        ok(stream.requestView(region, 12000));
        finish(stream, v);
        require(v.sceneInfo().cellCount == 3, "Falta alguna celda progresiva.");
        const auto cold = stream.info();
        require(cold.cacheMisses == 3 && cold.cacheEntries > 0, "Caché fría incorrecta.");
        ok(stream.requestView(region, 12000));
        finish(stream, v);
        require(stream.info().cacheHits >= 3, "La segunda vista vuelve a decodificar.");
        ok(v.fitCharts() ? enc::Status{}
                         : enc::Status{enc::ErrorCode::Decode, "No hay encuadre", {}});
        require(!v.visibleCells().empty() && !v.depthInfo().cells.empty(),
                "La vista cargada no informa celdas o contornos.");
        const auto depth = v.depthInfo();
        for (const auto& cell : depth.cells)
            require(!cell.contour || *cell.contour >= depth.requestedContour,
                    "El contorno mostrado es menor que el solicitado.");
        v.setChartsVisible(false);
        require(v.visibleCells().empty() && v.depthInfo().cells.empty() && !v.scaleInfo().overscale,
                "Las cartas ocultas todavía se informan como visibles.");
        v.setChartsVisible(true);
        const auto buoys = v.queryFeatures({}, "BOYLAT");
        require(!buoys.empty(), "Faltan boyas.");
        ok(v.fitFeatures({buoys[0].id}));
        if (buoys[0].position) {
            const auto screen = v.geoToScreen(*buoys[0].position);
            require(!v.pickAll(screen, 12).empty(), "Selección múltiple vacía sobre una boya.");
        }
        ok(stream.requestView({0, 0, 1, 1, true}));
        finish(stream, v);
        require(v.sceneInfo().cellCount == 0, "Una vista vacía conserva cartas ajenas.");
        ok(stream.requestView(region));
        ok(stream.requestView({0, 0, 1, 1, true}));
        finish(stream, v);
        require(v.sceneInfo().cellCount == 0, "Se confirmó un trabajo sustituido.");
        ok(stream.requestView(region));
        stream.cancel();
        while (stream.info().loading) {
            stream.poll(v);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(v.sceneInfo().cellCount == 0, "Cancelar reemplazó la escena.");
        enc::StreamOptions tiny;
        tiny.cacheBudgetBytes = 1;
        enc::ChartStreamer bounded(catalog, tiny);
        ok(bounded.requestView(region));
        finish(bounded, v);
        require(bounded.info().cacheBytes <= 1 && bounded.info().cacheEntries == 0,
                "La caché supera su presupuesto.");
        // Un índice rápido todavía no conoce la huella; la lectura no debe perder sus celdas.
        enc::ScanOptions quickOptions;
        quickOptions.inspectMetadata = false;
        enc::Catalog quickCatalog;
        ok(quickCatalog.scan(argv[1], quickOptions));
        enc::ChartStreamer quick(quickCatalog);
        ok(quick.requestView(region));
        finish(quick, v);
        require(v.sceneInfo().cellCount == 3,
                "El catálogo sin metadatos impide cargar celdas progresivamente.");
        ok(quick.requestView({0, 0, 1, 1, true}));
        finish(quick, v);
        require(v.sceneInfo().cellCount == 0 && quick.info().cacheHits >= 3,
                "Las celdas sin envolvente conocida no se filtran tras la lectura.");
        std::cout << "PASS: Mercator, antimeridiano, rotación, historial, escala, reglas, "
                     "selección múltiple y carga progresiva con índice completo o rápido.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
