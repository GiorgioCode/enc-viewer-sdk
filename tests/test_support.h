// test_support.h — Infraestructura EGL y capturas para verificar la API pública del SDK.
// Las pruebas crean su propio contexto, igual que una aplicación receptora, sin utilizar GLFW.
#pragma once
#include <EGL/egl.h>
#include <enc/enc.h>
#include <vector>
namespace enc_test {
using Pixels = std::vector<unsigned char>;
void require(bool condition, const std::string& message);
void ok(const enc::Status& status);
struct Context {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = nullptr;
    Context();
    ~Context();
    void activate();
    void detach();
};
Pixels capture(enc::Viewer& viewer, const enc::RenderTarget& target = {0, 0, 1280, 800});
unsigned pixel(const Pixels& pixels, int width, int height, int x, int y);
unsigned geographic(enc::Viewer& viewer, const Pixels& pixels, enc::GeoPoint point);
std::size_t countColour(const Pixels& pixels, unsigned colour);
std::size_t phosphor(const Pixels& pixels, bool red);
double brightness(const Pixels& pixels);
// Tolera únicamente bordes aliased mínimos; los estados se comprueban por separado.
bool similar(const Pixels& first, const Pixels& second);
void save(const Pixels& pixels, int width, int height, const std::filesystem::path& path);
void fixture(const std::filesystem::path& path, int epsg = 4326, bool rotated = false,
             bool georeferenced = true, bool gray16 = false, bool palette = false,
             bool gcps = false);
} // namespace enc_test
