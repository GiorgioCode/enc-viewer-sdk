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
    const auto info = viewer.sceneInfo();
    std::cout << "Paquete " << enc::version() << ": " << info.cellCount << " celdas / "
              << info.featureCount << " objetos\n";
    return info.cellCount == 3 && info.featureCount > 6000 ? 0 : 4;
}
