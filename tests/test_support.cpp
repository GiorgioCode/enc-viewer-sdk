// test_support.cpp — Contexto de compatibilidad sin pantalla y utilidades de píxeles.
// Permite comprobar composición, coordenadas y contraste usando el mismo contrato que un integrador.
#include "test_support.h"
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace enc_test {
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
void ok(const enc::Status& status) {
    require(static_cast<bool>(status), status.message);
}
Context::Context() {
    display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    require(display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr),
            "No se pudo inicializar EGL.");
    require(eglBindAPI(EGL_OPENGL_API), "No se pudo seleccionar OpenGL.");
    const EGLint attributes[] = {EGL_SURFACE_TYPE,
                                 EGL_PBUFFER_BIT,
                                 EGL_RENDERABLE_TYPE,
                                 EGL_OPENGL_BIT,
                                 EGL_RED_SIZE,
                                 8,
                                 EGL_GREEN_SIZE,
                                 8,
                                 EGL_BLUE_SIZE,
                                 8,
                                 EGL_NONE};
    EGLint count = 0;
    require(eglChooseConfig(display, attributes, &config, 1, &count) && count == 1,
            "Configuración EGL no disponible.");
    const EGLint size[] = {EGL_WIDTH, 2560, EGL_HEIGHT, 1600, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, size);
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, nullptr);
    require(surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT,
            "No se pudo crear el contexto EGL.");
    activate();
}
void Context::activate() {
    require(eglMakeCurrent(display, surface, surface, context), "No se pudo activar el contexto.");
}
void Context::detach() {
    require(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT),
            "No se pudo desactivar el contexto.");
}
Context::~Context() {
    detach();
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
}
Pixels capture(enc::Viewer& viewer, const enc::RenderTarget& target) {
    ok(viewer.render(target));
    glFinish();
    Pixels pixels(static_cast<std::size_t>(target.width) * target.height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(target.x, target.y, target.width, target.height, GL_RGB, GL_UNSIGNED_BYTE,
                 pixels.data());
    require(glGetError() == GL_NO_ERROR, "Error OpenGL al renderizar o capturar.");
    return pixels;
}
unsigned pixel(const Pixels& pixels, int width, int height, int x, int y) {
    require(x >= 0 && y >= 0 && x < width && y < height, "Consulta de píxel fuera de la imagen.");
    const auto i = (static_cast<std::size_t>(height - 1 - y) * width + x) * 3;
    return (static_cast<unsigned>(pixels[i]) << 16) | (static_cast<unsigned>(pixels[i + 1]) << 8) |
           pixels[i + 2];
}
unsigned geographic(enc::Viewer& viewer, const Pixels& pixels, enc::GeoPoint point) {
    const auto p = viewer.geoToScreen(point);
    return pixel(pixels, 1280, 800, static_cast<int>(p.x), static_cast<int>(p.y));
}
std::size_t countColour(const Pixels& pixels, unsigned colour) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 3)
        if (((static_cast<unsigned>(pixels[i]) << 16) |
             (static_cast<unsigned>(pixels[i + 1]) << 8) | pixels[i + 2]) == colour)
            ++count;
    return count;
}
std::size_t phosphor(const Pixels& pixels, bool red) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 3) {
        const int r = pixels[i], g = pixels[i + 1], b = pixels[i + 2];
        if (red ? r > 60 && r > g * 2 && r > b * 2 : g > 60 && g > r * 2 && g > b * 1.5)
            ++count;
    }
    return count;
}
double brightness(const Pixels& pixels) {
    double sum = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 3)
        sum += .2126 * pixels[i] + .7152 * pixels[i + 1] + .0722 * pixels[i + 2];
    return sum / (pixels.size() / 3);
}
bool similar(const Pixels& first, const Pixels& second) {
    if (first.size() != second.size())
        return false;
    std::size_t differences = 0;
    for (std::size_t i = 0; i < first.size(); i += 3)
        if (first[i] != second[i] || first[i + 1] != second[i + 1] || first[i + 2] != second[i + 2])
            ++differences;
    return differences <= first.size() / 3 / 5000;
}
void save(const Pixels& pixels, int width, int height, const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output), "No se pudo guardar captura.");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int row = height - 1; row >= 0; --row)
        output.write(reinterpret_cast<const char*>(pixels.data() +
                                                   static_cast<std::size_t>(row) * width * 3),
                     static_cast<std::streamsize>(width) * 3);
}
} // namespace enc_test
