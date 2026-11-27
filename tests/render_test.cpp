// render_test.cpp — Verificación gráfica de la biblioteca a través de su API pública.
// Contrasta mundo, ENC, GeoTIFF múltiples, radar diurno/nocturno y reutilización del contexto anfitrión.
#define GL_GLEXT_PROTOTYPES
#include "test_support.h"
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <gdal_priv.h>
#include <iostream>
#include <thread>
using namespace enc_test;
namespace {
void checkImages(enc::Viewer& viewer, const std::filesystem::path& temp) {
    const auto baseline = capture(viewer);
    const auto red = temp / "red.tif", blue = temp / "blue.tif";
    fixture(red);
    fixture(blue);
    // Cambia sólo RGB para comprobar inequívocamente el orden entre dos imágenes georreferenciadas.
    auto* blueData = static_cast<GDALDataset*>(GDALOpen(blue.string().c_str(), GA_Update));
    require(blueData, "No se pudo abrir el TIFF azul.");
    for (int band = 1; band <= 3; ++band) {
        const unsigned char colours[] = {20, 40, 230};
        std::vector<unsigned char> values(64 * 48, colours[band - 1]);
        require(blueData->GetRasterBand(band)->RasterIO(GF_Write, 0, 0, 64, 48, values.data(), 64,
                                                        48, GDT_Byte, 0, 0, nullptr) == CE_None,
                "No se pudo escribir el TIFF azul.");
    }
    GDALClose(blueData);
    enc::RasterOptions opaque;
    opaque.opacity = 1;
    const auto first = viewer.addGeoTiff(red, opaque);
    require(static_cast<bool>(first), first.status.message);
    const auto redFrame = capture(viewer);
    require(geographic(viewer, redFrame, {-71.04, 42.355}) == 0xE6140A,
            "TIFF con orientación o colores incorrectos.");
    require(geographic(viewer, redFrame, {-71.057, 42.367}) ==
                geographic(viewer, baseline, {-71.057, 42.367}),
            "El alfa TIFF tapa la carta.");
    const auto second = viewer.addGeoTiff(blue, opaque);
    require(static_cast<bool>(second), second.status.message);
    require(geographic(viewer, capture(viewer), {-71.04, 42.355}) == 0x1428E6,
            "La segunda imagen no se compone arriba.");
    ok(viewer.moveRasterToTop(first.value));
    require(geographic(viewer, capture(viewer), {-71.04, 42.355}) == 0xE6140A,
            "No se puede reordenar imágenes.");
    ok(viewer.setRasterVisible(first.value, false));
    require(geographic(viewer, capture(viewer), {-71.04, 42.355}) == 0x1428E6,
            "Ocultar una imagen afecta las demás.");
    ok(viewer.setRasterOpacity(second.value, 0));
    require(geographic(viewer, capture(viewer), {-71.04, 42.355}) ==
                geographic(viewer, baseline, {-71.04, 42.355}),
            "Alfa cero no recupera el fondo.");
    ok(viewer.removeRaster(first.value));
    ok(viewer.removeRaster(second.value));
    // Los tipos de TIFF comparten la misma API: su geometría y máscara deben llegar al dibujado.
    struct Variant {
        const char* name;
        int epsg;
        bool rotated, georeferenced, gray, palette, gcp;
    };
    for (const auto& variant :
         std::vector<Variant>{{"manual", 4326, false, false, false, false, false},
                              {"projected", 3857, true, true, false, false, false},
                              {"gray16", 4326, false, true, true, false, false},
                              {"palette", 4326, false, true, false, true, false},
                              {"gcp", 4326, false, true, false, false, true}}) {
        const auto path = temp / (std::string(variant.name) + ".tif");
        fixture(path, variant.epsg, variant.rotated, variant.georeferenced, variant.gray,
                variant.palette, variant.gcp);
        const auto added = viewer.addGeoTiff(path, opaque);
        require(static_cast<bool>(added), added.status.message);
        const auto info = viewer.rasters().back();
        require(info.georeferenced == variant.georeferenced, "Indicador geográfico incorrecto.");
        require(capture(viewer) != baseline, "La variante TIFF no se dibuja.");
        ok(viewer.removeRaster(added.value));
    }
}
void checkRadar(enc::Viewer& viewer, const std::filesystem::path& output) {
    const auto baseline = capture(viewer);
    viewer.enableRadar(true);
    require(viewer.radarInfo().echoCount > 100, "No se construyeron ecos de las cartas.");
    const auto first = capture(viewer);
    require(phosphor(first, false) > phosphor(baseline, false) + 200,
            "PPI diurno sin ecos verdes.");
    for (int i = 0; i < 8; ++i)
        ok(viewer.advance(.1));
    const auto moved = capture(viewer);
    require(first != moved, "El radar no avanza.");
    auto settings = viewer.radarInfo().settings;
    settings.paused = true;
    ok(viewer.configureRadar(settings));
    const auto frozenTime = viewer.radarInfo().seconds;
    const auto frozen = capture(viewer);
    ok(viewer.advance(.2));
    require(viewer.radarInfo().seconds == frozenTime && similar(capture(viewer), frozen),
            "La pausa altera el tiempo o los ecos.");
    const auto camera = viewer.camera();
    viewer.setTheme(enc::Theme::Night);
    const auto night = capture(viewer);
    require(phosphor(night, true) > 500 &&
                viewer.camera().pixelsPerDegree == camera.pixelsPerDegree &&
                viewer.radarInfo().seconds == frozenTime,
            "Radar nocturno sin rojo o cambio de estado.");
    save(night, 1280, 800, output / "night-radar.ppm");
    viewer.setTheme(enc::Theme::Day);
    save(capture(viewer), 1280, 800, output / "day-radar.ppm");
    const auto oldRange = settings.rangeNm;
    ok(viewer.changeRadarRange(1));
    require(viewer.radarInfo().settings.rangeNm >= oldRange, "Alcance no modificable.");
    // La alimentación externa no debe mezclarse con ruido ni blancos sintéticos.
    settings = viewer.radarInfo().settings;
    settings.mode = enc::RadarMode::ExternalEchoes;
    settings.origin = {-71.04, 42.355};
    settings.rangeNm = 1.5;
    ok(viewer.configureRadar(settings));
    ok(viewer.setRadarEchoes({{{-71.04, 42.355}, 1}, {{-71.035, 42.36}, .7f}}));
    require(viewer.radarInfo().echoCount == 2, "La escena externa contiene ecos inventados.");
    capture(viewer);
    require(!viewer.setRadarEchoes({{{-71, 42}, -1}}) && viewer.radarInfo().echoCount == 2,
            "Un eco inválido reemplazó la escena externa.");
    viewer.enableRadar(false);
}
void checkCoreRejection(Context& context, enc::Viewer& viewer) {
    const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION,
                                 3,
                                 EGL_CONTEXT_MINOR_VERSION,
                                 3,
                                 EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                 EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                 EGL_NONE};
    auto core = eglCreateContext(context.display, context.config, EGL_NO_CONTEXT, attributes);
    if (core == EGL_NO_CONTEXT) {
        std::cout << "SKIP: controlador sin contexto core 3.3.\n";
        return;
    }
    require(eglMakeCurrent(context.display, context.surface, context.surface, core),
            "No se pudo activar el contexto core de prueba.");
    const auto status = viewer.render({0, 0, 1280, 800});
    require(status.code == enc::ErrorCode::Graphics,
            "Un contexto core no se rechazó con diagnóstico explícito.");
    require(glGetError() == GL_NO_ERROR,
            "Se ejecutaron primitivas incompatibles antes de rechazar el contexto core.");
    context.activate();
    eglDestroyContext(context.display, core);
}
void checkHostState(enc::Viewer& viewer, const std::filesystem::path& output) {
    ok(viewer.releaseGraphicsResources());
    ok(viewer.resize(640, 480));
    enc::RasterOptions opaque;
    opaque.opacity = 1;
    const auto image = viewer.addGeoTiff(output / "red.tif", opaque);
    require(static_cast<bool>(image), image.status.message);
    const enc::RenderTarget region{70, 50, 640, 480};
    // FBO propio: el SDK debe respetar el destino activo y limitarse a su subregión.
    GLuint framebuffer = 0, colour = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &colour);
    glBindTexture(GL_TEXTURE_2D, colour);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 900, 650, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
            "FBO de prueba incompleto.");
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(1, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    const auto expected = capture(viewer, region);
    ok(viewer.releaseGraphicsResources()); // Fuerza una subida TIFF con PBO del anfitrión enlazada.
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    const char* source =
        "#version 120\nvoid main(){gl_Position=ftransform();gl_FrontColor=gl_Color;}";
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    const auto program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glUseProgram(program);
    GLuint pbo = 0;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 64, nullptr, GL_STATIC_DRAW);
    glViewport(3, 4, 115, 117);
    glEnable(GL_SCISSOR_TEST);
    glScissor(9, 10, 11, 12);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ZERO);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslated(3, 4, 0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glScaled(2, 3, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(5, 6, 0);
    glMatrixMode(GL_TEXTURE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 99);
    const auto actual = capture(viewer, region);
    require(similar(actual, expected),
            "El estado del anfitrión contamina el dibujo de la biblioteca.");
    GLint integer = 0, box[4];
    GLboolean mask[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &integer);
    require(static_cast<GLuint>(integer) == framebuffer, "El SDK cambió el framebuffer.");
    glGetIntegerv(GL_VIEWPORT, box);
    require(box[0] == 3 && box[1] == 4 && box[2] == 115 && box[3] == 117,
            "No se restauró viewport.");
    glGetIntegerv(GL_SCISSOR_BOX, box);
    require(glIsEnabled(GL_SCISSOR_TEST) && box[0] == 9 && box[1] == 10 && box[2] == 11 &&
                box[3] == 12,
            "No se restauró scissor.");
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    require(!mask[0] && mask[1] && !mask[2] && mask[3], "No se restauró máscara de color.");
    glGetIntegerv(GL_ACTIVE_TEXTURE, &integer);
    require(integer == GL_TEXTURE1 && glIsEnabled(GL_TEXTURE_2D),
            "No se restauró unidad de textura.");
    glGetIntegerv(GL_CURRENT_PROGRAM, &integer);
    require(static_cast<GLuint>(integer) == program, "No se restauró programa shader.");
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &integer);
    require(static_cast<GLuint>(integer) == pbo, "No se restauró PBO.");
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &integer);
    require(integer == 99, "No se restauró almacenamiento de píxeles.");
    glGetIntegerv(GL_MATRIX_MODE, &integer);
    require(integer == GL_TEXTURE, "No se restauró modo de matriz.");
    GLdouble matrix[16];
    glGetDoublev(GL_TEXTURE_MATRIX, matrix);
    require(matrix[12] == 3 && matrix[13] == 4, "Se alteró la matriz de textura.");
    std::array<unsigned char, 3> outside{};
    glReadPixels(1, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, outside.data());
    require(outside == std::array<unsigned char, 3>{255, 0, 255},
            "La biblioteca pintó fuera de su región.");
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glDeleteBuffers(1, &pbo);
    glUseProgram(0);
    glDeleteProgram(program);
    glDeleteShader(shader);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &colour);
    ok(viewer.removeRaster(image.value));
    require(glGetError() == GL_NO_ERROR, "Error durante la prueba del contrato OpenGL.");
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Se requieren carpeta ENC y carpeta de capturas.");
        const std::filesystem::path output = argv[2];
        std::filesystem::create_directories(output);
        Context context;
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';
        enc::Viewer viewer;
        ok(viewer.resize(1280, 800));
        viewer.fitWorld();
        const auto world = capture(viewer);
        require(geographic(viewer, world, {22, 4}) == 0xDADCC8 &&
                    geographic(viewer, world, {-145, -5}) == 0xB7D6DE,
                "Mapa mundial incorporado incompleto.");
        save(world, 1280, 800, output / "world.ppm");
        ok(viewer.loadFolder(argv[1]));
        const auto day = capture(viewer);
        require(countColour(day, 0xE2D3A6) > 20000 &&
                    countColour(day, 0x8EC2CE) + countColour(day, 0xA8CFD5) > 10000,
                "Cartas sin tierra o profundidades.");
        save(day, 1280, 800, output / "day-charts.ppm");
        viewer.setTheme(enc::Theme::Night);
        const auto night = capture(viewer);
        require(brightness(night) < brightness(day) * .45, "La carta nocturna no se oscurece.");
        save(night, 1280, 800, output / "night-charts.ppm");
        viewer.setTheme(enc::Theme::Day);
        viewer.setChartsVisible(false);
        const auto noCharts = capture(viewer);
        require(noCharts != day && viewer.sceneInfo().cellCount == 3,
                "El interruptor ENC no funciona.");
        viewer.setChartsVisible(true);
        const auto restored = capture(viewer);
        require(similar(restored, day), "El interruptor no restaura las cartas.");
        checkImages(viewer, output);
        checkRadar(viewer, output);
        viewer.enableRadar(false);
        viewer.setTheme(enc::Theme::Day);
        const auto demo = viewer.addDemoGeoTiff();
        require(static_cast<bool>(demo), demo.status.message);
        save(capture(viewer), 1280, 800, output / "geotiff.ppm");
        // Liberar y volver a dibujar debe reconstruir texturas a partir de los datos CPU conservados.
        const auto beforeRelease = capture(viewer);
        ok(viewer.releaseGraphicsResources());
        require(similar(capture(viewer), beforeRelease),
                "La textura no se recupera tras liberar GPU.");
        context.detach();
        ok(viewer.removeRaster(demo.value));
        context.activate();
        capture(viewer);
        checkHostState(viewer, output);
        ok(viewer.resize(1280, 800));
        viewer.fitCharts();
        const auto hidpi = capture(viewer, {0, 0, 2560, 1600});
        require(hidpi.size() == 2560u * 1600u * 3u, "No se respeta HiDPI.");
        ok(viewer.renderSymbolCatalogue({0, 0, 1280, 800}));
        glFinish();
        Pixels catalogue(1280 * 800 * 3);
        glReadPixels(0, 0, 1280, 800, GL_RGB, GL_UNSIGNED_BYTE, catalogue.data());
        save(catalogue, 1280, 800, output / "symbols.ppm");
        enc::Status otherThread;
        std::thread wrongThread([&] { otherThread = viewer.render({0, 0, 1280, 800}); });
        wrongThread.join();
        require(!otherThread, "Se permitió dibujar sin contexto en otro hilo.");
        ok(viewer.releaseGraphicsResources());
        ok(viewer.releaseGraphicsResources());
        checkCoreRejection(context, viewer);
        require(glGetError() == GL_NO_ERROR, "El SDK dejó un error gráfico pendiente.");
        std::cout << "PASS: mundo, ENC, día/noche, GeoTIFF múltiples y variantes, radar, contexto "
                     "externo, FBO, estado OpenGL, cola GPU e HiDPI.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
