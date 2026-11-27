// main.cpp — Prueba de que una aplicación externa puede enlazar y utilizar el paquete instalado.
// Sólo incluye contratos públicos y carga datos recibidos como argumento para evitar rutas internas.
#include <enc/enc.h>
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 2)
        return 1;
    enc::Catalog catalog;
    auto indexed = catalog.scan(argv[1]);
    if (!indexed) {
        std::cerr << indexed.message << '\n';
        return 2;
    }
    enc::Viewer viewer;
    auto loaded = viewer.loadCatalog(catalog);
    if (!loaded) {
        std::cerr << loaded.message << '\n';
        return 3;
    }
    // El consumidor también enlaza las APIs 0.2 desde el paquete trasladado.
    auto camera = viewer.camera();
    camera.projection = enc::Projection::Mercator;
    camera.rotationDegrees = 15;
    if (!viewer.setCamera(camera) || !viewer.setPortrayalRules({}))
        return 5;
    enc::Contact report;
    report.id = "consumer-ais";
    report.position = viewer.camera().center;
    if (!viewer.upsertContacts({report}) || viewer.contacts().size() != 1 ||
        viewer.visibleCells().empty() || viewer.scaleInfo().denominator <= 0 ||
        enc::objectClassDescription("BOYLAT").empty())
        return 6;
    enc::ChartStreamer stream(catalog);
    if (!stream.requestView(viewer.viewBounds(), viewer.scaleInfo().denominator))
        return 7;
    stream.cancel();
    const auto info = viewer.sceneInfo();
    std::cout << "Paquete " << enc::version() << ": " << info.cellCount << " celdas / "
              << info.featureCount << " objetos\n";
    return info.cellCount == 3 && info.featureCount > 6000 ? 0 : 4;
}
