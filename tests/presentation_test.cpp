// presentation_test.cpp — Prueba gráfica de mosaico con huecos, reglas, texto Unicode y contactos.
// Combina una escena sintética controlada con la API pública para verificar símbolos y recursos GPU.
#include "detail/app_state.h"
#include "detail/camera.h"
#include "detail/chart_renderer.h"
#include "detail/geometry.h"
#include "detail/mosaic.h"
#include "detail/overlay_renderer.h"
#include "detail/picking.h"
#include "detail/text_renderer.h"
#include "test_support.h"
#include <GL/gl.h>
#include <algorithm>
#include <iostream>
using namespace enc_test;
namespace {
enc::detail::Polygon rectangle(double a, double b, double c, double d) {
    enc::detail::Polygon p;
    p.rings = {{{a, b}, {c, b}, {c, d}, {a, d}}};
    enc::detail::triangulate(p);
    return p;
}
void mosaic() {
    using namespace enc::detail;
    SceneState s;
    s.width = s.height = s.framebufferWidth = s.framebufferHeight = 512;
    s.zoom = 5000;
    s.presentation.detailThreshold = 10;
    s.presentation.showScaleBar = s.presentation.showOverscale = false;
    CellData coarse;
    coarse.path = "coarse.000";
    coarse.compilationScale = 50000;
    coarse.hasCoverage = true;
    coarse.coverage = {rectangle(-.04, -.04, .04, .04)};
    coarse.bounds = {-.04, -.04, .04, .04, true};
    CellData fine = coarse;
    fine.path = "fine.000";
    fine.compilationScale = 10000;
    fine.coverage = {rectangle(-.02, -.02, .02, .02)};
    fine.coverage[0].rings.push_back({{-.005, -.005}, {.005, -.005}, {.005, .005}, {-.005, .005}});
    fine.coverage[0].triangles.clear();
    triangulate(fine.coverage[0]);
    s.cells = {coarse, fine};
    s.layers = {{"coarse", 1, true, "DEPARE", "coarse.000"},
                {"fine", 1, true, "DEPARE", "fine.000"}};
    Feature first;
    first.polygons = coarse.coverage;
    first.attributes = {{"DRVAL1", "1"}};
    first.minX = first.minY = -.04;
    first.maxX = first.maxY = .04;
    Feature second = first;
    second.cell = second.layer = 1;
    second.polygons = fine.coverage;
    second.attributes = {{"DRVAL1", "20"}};
    s.features = {first, second};
    s.spatial.rebuild(s.features);
    require(visibleAt(s, 0, {0, 0}) && !visibleAt(s, 0, {.01, 0}) && visibleAt(s, 1, {.01, 0}),
            "La máscara de cobertura ignora huecos o detalle.");
    glViewport(0, 0, 512, 512);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 512, 512, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    drawChart(s);
    glFinish();
    Pixels pixels(512 * 512 * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 512, 512, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    require(pixel(pixels, 512, 512, 256, 256) == 0x8EC2CE,
            "La celda detallada tapa el hueco con datos inexistentes.");
    require(pixel(pixels, 512, 512, 306, 256) == 0xC8E1DF,
            "La celda general prevalece sobre la detallada.");
    for (auto& rule : s.rules)
        if (rule.objectClass == "DEPARE")
            rule.fill = 0x123456;
    drawChart(s);
    glReadPixels(0, 0, 512, 512, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    require(pixel(pixels, 512, 512, 306, 256) == 0x123456,
            "El color configurable no llega al renderizador.");
    require(glGetError() == GL_NO_ERROR, "Error OpenGL en mosaico.");
    require(textWidth("Águila") > 0 && textWidth("Águila") < textWidth("Aguilaaaa"),
            "Medición Unicode cuenta bytes.");
    require(decodeUtf8("Á").size() == 1 && decodeUtf8("\xff")[0] == 0xfffd,
            "Decodificación UTF-8 inválida.");
}
void rasterDateLine() {
    using namespace enc::detail;
    SceneState scene;
    scene.width = scene.height = scene.framebufferWidth = scene.framebufferHeight = 512;
    scene.zoom = 1000;
    RasterLayer layer;
    layer.image.width = layer.image.height = 1;
    layer.image.rgba = {255, 0, 255, 255};
    layer.image.opacity = 1;
    layer.image.corners = {{{179.9, .1}, {-179.9, .1}, {-179.9, -.1}, {179.9, -.1}}};
    scene.rasters.push_back(std::move(layer));
    glViewport(0, 0, 512, 512);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 512, 512, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    auto captureRaster = [&] {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        drawOverlays(scene);
        glFinish();
        Pixels pixels(512 * 512 * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, 512, 512, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    for (auto projection : {enc::Projection::LocalEquirectangular, enc::Projection::Mercator})
        for (double rotation : {0., 45.}) {
            scene.projection = projection;
            scene.rotation = rotation;
            scene.centerX = 0;
            require(pixel(captureRaster(), 512, 512, 256, 256) == 0,
                    "El TIFF del antimeridiano cruza incorrectamente el centro del mundo.");
            scene.centerX = 180 * lonScale(scene);
            require(pixel(captureRaster(), 512, 512, 256, 256) == 0xFF00FF,
                    "El TIFF sobre ±180° no se dibuja junto al antimeridiano.");
        }
    require(glGetError() == GL_NO_ERROR, "Error OpenGL en TIFF del antimeridiano.");
    scene.gpu->release();
}
void targets(const std::filesystem::path& output) {
    enc::Viewer v;
    ok(v.resize(1280, 800));
    ok(v.fitBounds({-.02, -.02, .02, .02, true}));
    enc::Contact c;
    c.id = "ais";
    c.name = "Águila";
    c.position = {0, 0};
    c.heading = 0;
    c.courseOverGround = 90;
    c.speedOverGround = 10;
    c.state = enc::ContactState::Activated;
    ok(v.upsertContacts({c}));
    const auto active = capture(v);
    const auto center = v.geoToScreen({0, 0});
    auto green = [&](const Pixels& image) {
        std::size_t n = 0;
        for (int y = int(center.y) - 70; y < int(center.y) + 30; ++y)
            for (int x = int(center.x) - 30; x < int(center.x) + 30; ++x)
                if (pixel(image, 1280, 800, x, y) == 0x006B44)
                    ++n;
        return n;
    };
    require(green(active) > 20, "Falta el triángulo AIS o heading.");
    c.heading = 90;
    ok(v.upsertContacts({c}));
    require(capture(v) != active, "Heading no orienta el símbolo.");
    c.dangerous = true;
    ok(v.upsertContacts({c}));
    auto danger = capture(v);
    require(countColour(danger, 0xFF3030) > 50, "Peligro no es rojo sólido.");
    ok(v.advanceContacts(.6));
    require(capture(v) != danger, "El peligro sin reconocer no parpadea.");
    ok(v.acknowledgeContact(c.id));
    require(countColour(capture(v), 0xFF3030) > 50,
            "Reconocer elimina el peligro en vez de fijarlo.");
    c.source = enc::ContactSource::Radar;
    c.dangerous = false;
    ok(v.upsertContacts({c}));
    const auto radar = capture(v);
    require(radar != active, "Radar y AIS comparten un símbolo.");
    c.state = enc::ContactState::Lost;
    ok(v.upsertContacts({c}));
    ok(v.acknowledgeContact(c.id));
    require(!v.contact(c.id)->visible, "El perdido reconocido sigue dibujado.");
    c.state = enc::ContactState::Activated;
    c.source = enc::ContactSource::Associated;
    ok(v.upsertContacts({c}));
    v.setTheme(enc::Theme::Night);
    const auto night = capture(v);
    require(countColour(night, 0x51D69B) > 20, "Contacto nocturno pierde su color semántico.");
    save(night, 1280, 800, output / "contacts-night.ppm");
    save(danger, 1280, 800, output / "contacts-danger.ppm");
    ok(v.releaseGraphicsResources());
    require(similar(capture(v), night), "No se reconstruye el atlas tras liberar el contexto.");
    ok(v.releaseGraphicsResources());
}
void targetSemantics() {
    enc::Viewer v;
    ok(v.resize(1280, 800));
    ok(v.fitBounds({-.02, -.02, .02, .02, true}));
    auto settings = v.contactSettings();
    settings.showNames = settings.showPastPositions = false;
    ok(v.configureContacts(settings));
    enc::Contact c;
    c.id = "automatic-radar";
    c.source = enc::ContactSource::Radar;
    c.state = enc::ContactState::Acquiring;
    c.position = {0, 0};
    c.automaticAcquisition = true;
    ok(v.upsertContacts({c}));
    const auto center = v.geoToScreen(c.position);
    auto region = [](const Pixels& image, int x1, int y1, int x2, int y2, unsigned colour) {
        std::size_t n = 0;
        for (int y = y1; y < y2; ++y)
            for (int x = x1; x < x2; ++x)
                n += pixel(image, 1280, 800, x, y) == colour;
        return n;
    };
    const auto acquiring = capture(v);
    require(region(acquiring, int(center.x) - 15, int(center.y) - 15, int(center.x) + 15,
                   int(center.y) + 15, 0xFF3030) > 20,
            "Adquisición automática no tiene segmentos rojos gruesos.");
    ok(v.acknowledgeContact(c.id));
    const auto acknowledged = capture(v);
    require(region(acknowledged, int(center.x) - 15, int(center.y) - 15, int(center.x) + 15,
                   int(center.y) + 15, 0xFF3030) == 0 &&
                region(acknowledged, int(center.x) - 15, int(center.y) - 15, int(center.x) + 15,
                       int(center.y) + 15, 0x006B44) > 10,
            "Reconocer la adquisición no restaura segmentos de color normal.");

    // El X global informa que hay blancos simulados aunque su símbolo parpadee o salga de la vista.
    c.state = enc::ContactState::Activated;
    c.dangerous = c.testTarget = true;
    ok(v.upsertContacts({c}));
    // El atlas usa filtrado lineal: una diagonal ampliada puede no tener píxeles del color exacto.
    auto marker = [&](const Pixels& image) {
        std::size_t count = 0;
        for (int y = 0; y < 70; ++y)
            for (int x = 1220; x < 1280; ++x) {
                const auto rgb = pixel(image, 1280, 800, x, y);
                const auto r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
                count += g > 40 && g > r * 1.2 && g > b * 1.1;
            }
        return count;
    };
    require(marker(capture(v)) > 0, "Falta aviso global del blanco de prueba.");
    ok(v.advanceContacts(.6));
    require(marker(capture(v)) > 0, "El aviso global de prueba parpadea junto al blanco.");
    c.position = {10, 10};
    ok(v.upsertContacts({c}));
    require(marker(capture(v)) > 0, "El aviso de prueba depende del blanco visible.");
    c.testTarget = false;
    ok(v.upsertContacts({c}));
    require(marker(capture(v)) == 0,
            "El aviso de prueba persiste después de desactivar el blanco.");

    // La opción de asociación decide qué familia de símbolos representa el mismo contacto.
    c.source = enc::ContactSource::Associated;
    c.dangerous = false;
    c.position = {0, 0};
    c.heading = 0;
    ok(v.upsertContacts({c}));
    const auto associatedAis = capture(v);
    settings.associatedAsAIS = false;
    ok(v.configureContacts(settings));
    require(capture(v) != associatedAis,
            "Cambiar representación del asociado no cambia el símbolo.");
    ok(v.releaseGraphicsResources());
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Falta carpeta de capturas.");
        std::filesystem::create_directories(argv[1]);
        Context context;
        mosaic();
        rasterDateLine();
        targets(argv[1]);
        targetSemantics();
        std::cout << "PASS: mosaico, huecos, texto UTF-8, símbolos de contactos, peligro y "
                     "reconstrucción GPU.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
