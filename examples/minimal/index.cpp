// index.cpp — Ejemplo mínimo de indexación y lectura ENC sin ventana ni contexto OpenGL.
// Consume exclusivamente la API pública; admite guardar un inventario JSON como segundo argumento.
#include <enc/enc.h>
#include <iostream>
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: enc-index CARPETA_O_CELDA [inventario.json]\n";
        return 1;
    }
    enc::Catalog catalog;
    const auto indexed = catalog.scan(argv[1]);
    if (!indexed) {
        std::cerr << indexed.message << '\n';
        return 2;
    }
    for (const auto& cell : catalog.cells())
        std::cout << cell.name << " | escala 1:" << cell.compilationScale << " | edición "
                  << cell.edition << " | actualización " << cell.updateNumber << " | "
                  << cell.updates.size() << " archivos de actualización\n";
    for (const auto& d : indexed.diagnostics)
        std::cerr << d.path << ": " << d.message << '\n';
    if (argc > 2) {
        const auto saved = catalog.saveInventory(argv[2]);
        if (!saved) {
            std::cerr << saved.message << '\n';
            return 3;
        }
    }
    enc::Viewer viewer;
    const auto loaded = viewer.loadCatalog(catalog);
    if (!loaded) {
        std::cerr << loaded.message << '\n';
        return 4;
    }
    const auto scene = viewer.sceneInfo();
    std::cout << scene.cellCount << " celdas / " << scene.featureCount << " objetos / "
              << scene.triangleCount << " triángulos\n";
    return 0;
}
