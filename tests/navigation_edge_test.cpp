// navigation_edge_test.cpp — Regresiones CPU de selección rotada y encuadre junto al antimeridiano.
// Usa escenas controladas para comprobar que el índice conserva candidatos visibles y símbolos grandes.
#include "detail/app_state.h"
#include "detail/camera.h"
#include "detail/geometry.h"
#include "detail/mosaic.h"
#include "detail/picking.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
// Más de dieciséis objetos fuerzan la partición del BVH: una envolvente errónea no queda disimulada.
void addBuoys(enc::detail::SceneState& scene, double screenX, double screenY) {
    using namespace enc::detail;
    scene.cells.push_back({});
    scene.layers.push_back({"Ayudas", 32, true, "BOYLAT", "test.000"});
    for (int i = 0; i < 32; ++i) {
        const auto point = worldAt(scene, screenX + i * .01, screenY);
        Feature feature;
        feature.points = {{point.lon, point.lat}};
        feature.minX = feature.maxX = point.lon;
        feature.minY = feature.maxY = point.lat;
        feature.symbol.kind = nautical::Kind::Buoy;
        scene.features.push_back(std::move(feature));
    }
    scene.spatial.rebuild(scene.features);
}
void rotatedPicking() {
    using namespace enc::detail;
    for (auto projection : {enc::Projection::LocalEquirectangular, enc::Projection::Mercator})
        for (double rotation : {0., 45., 90., 135., -45.}) {
            SceneState scene;
            scene.width = scene.height = 512;
            scene.zoom = 5000;
            scene.rotation = rotation;
            scene.projection = projection;
            scene.referenceLatitude = scene.centerLat = 60;
            scene.centerX = 179.999 * lonScale(scene);
            addBuoys(scene, 257, 255);
            require(hitTests(scene, 256, 256, 6).size() == 32,
                    "La selección rotada pierde ayudas próximas al cursor.");
        }
    SceneState enlarged;
    enlarged.width = enlarged.height = 512;
    enlarged.zoom = 10000;
    enlarged.presentation.symbolScale = 4;
    addBuoys(enlarged, 256, 356);
    require(hitTests(enlarged, 256, 256, 6).size() == 32,
            "La consulta espacial no incluye el anclaje de un símbolo ampliado.");
}
void rasterAtDateLine() {
    using namespace enc::detail;
    overlays::Raster image;
    image.width = image.height = 1;
    image.rgba = {0, 0, 0, 255};
    image.corners = {{{179.8, 60.1}, {-179.8, 60.1}, {-179.8, 59.9}, {179.8, 59.9}}};
    for (auto projection : {enc::Projection::LocalEquirectangular, enc::Projection::Mercator})
        for (double rotation : {0., 45., 90.}) {
            SceneState scene;
            scene.width = 900;
            scene.height = 700;
            scene.rotation = rotation;
            scene.projection = projection;
            fitRaster(scene, image);
            const auto centre = worldAt(scene, 450, 350);
            require(std::abs(centre.lon) > 179.9 && scene.zoom > 1000,
                    "El encuadre TIFF cruza todo el mundo en lugar del antimeridiano.");
            for (auto corner : image.corners) {
                const auto pixel = toScreen(scene, {corner.lon, corner.lat});
                require(pixel.x >= 0 && pixel.x <= 900 && pixel.y >= 0 && pixel.y <= 700,
                        "El encuadre rotado deja una esquina del TIFF fuera de pantalla.");
            }
        }
}
enc::detail::CellData cell(const char* path, int scale, double west, double east) {
    using namespace enc::detail;
    CellData result;
    result.path = path;
    result.compilationScale = scale;
    result.hasCoverage = true;
    result.bounds = {west, -.1, east, .1, true};
    Polygon polygon;
    polygon.rings = {{{west, -.1}, {east, -.1}, {east, .1}, {west, .1}}};
    triangulate(polygon);
    result.coverage.push_back(std::move(polygon));
    return result;
}
void mosaicSelection() {
    using namespace enc::detail;
    SceneState scene;
    scene.width = scene.height = 512;
    scene.zoom = 5000;
    scene.presentation.detailThreshold = 100;
    scene.cells = {cell("coarse.000", 500000, -.1, .1), cell("fine.000", 10000, -.055, .055)};
    require(activeCells(scene, 0) == std::vector<std::size_t>{1},
            "Una carta completamente oculta todavía figura como visible.");
    require(activeCells(scene, 50) == std::vector<std::size_t>({0, 1}),
            "El margen del render no conserva la cobertura exterior al viewport.");
    const auto cache = scene.mosaicCache;
    require(activeCells(scene, 0) == std::vector<std::size_t>{1} && cache == scene.mosaicCache,
            "La consulta sin cambios descarta la caché de mosaico.");
    scene.zoom *= 2;
    activeCells(scene, 0);
    require(cache != scene.mosaicCache, "El cambio de cámara conserva un mosaico obsoleto.");
    scene.zoom = 5000;
    scene.cells[1].coverage[0].rings.push_back(
        {{-.005, -.005}, {.005, -.005}, {.005, .005}, {-.005, .005}});
    scene.mosaicCache.reset();
    require(activeCells(scene, 0) == std::vector<std::size_t>({0, 1}),
            "El hueco de la carta detallada no conserva la general.");
    scene.cells = {cell("coarse.000", 50000, -.04, -.01), cell("fine.000", 10000, .01, .04)};
    scene.mosaicCache.reset();
    scene.presentation.detailThreshold = 1;
    require(activeCells(scene, 0) == std::vector<std::size_t>({0, 1}),
            "La escala elimina la única cobertura de una región vecina.");
    scene.cells[0] = cell("coarse.000", 50000, -.04, .04);
    scene.mosaicCache.reset();
    require(activeCells(scene, 0) == std::vector<std::size_t>{0},
            "El detalle subescala no cede a la cobertura general disponible.");
}
void continuousDateLine() {
    using namespace enc::detail;
    SceneState scene;
    scene.width = 900;
    scene.height = 512;
    scene.zoom = 2;
    scene.cells = {cell("centre.000", 50000, -.1, .1), cell("dateline.000", 10000, 179.8, 180.2)};
    require(activeCells(scene, 0) == std::vector<std::size_t>({0, 1}),
            "La cobertura del antimeridiano tapa incorrectamente el centro del mundo.");
    const auto a = toScreenContinuous(scene, {179.8, 0}, 179.8);
    const auto b = toScreenContinuous(scene, {180.2, 0}, 179.8);
    require(std::abs(a.x - b.x) < 1, "La geometría continua se parte por ±180°.");
    SceneState picking;
    picking.width = picking.height = 512;
    picking.zoom = 1000;
    picking.cells.push_back({});
    picking.layers.push_back({"Líneas", 2, true, "COALNE", "test.000"});
    Feature line;
    line.lines = {{{179.8, 0}, {180.2, 0}}};
    line.minX = 179.8;
    line.maxX = 180.2;
    Feature point;
    point.points = {{0, 0}};
    picking.features = {line, point};
    picking.spatial.rebuild(picking.features);
    const auto hits = hitTests(picking, 256, 256, 6);
    require(std::find(hits.begin(), hits.end(), 0) == hits.end(),
            "Una línea sobre ±180° aparece seleccionable por el centro del mapa.");
}
} // namespace
int main() {
    try {
        rotatedPicking();
        rasterAtDateLine();
        mosaicSelection();
        continuousDateLine();
        std::cout << "PASS: selección rotada, símbolos ampliados, mosaico por cobertura y ±180°.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
