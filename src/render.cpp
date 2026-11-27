// render.cpp — Frontera OpenGL del SDK y aislamiento del estado de la aplicación anfitriona.
// Dibuja en el framebuffer activo y una región física explícita; no crea contexto ni presenta buffers.
#define GL_GLEXT_PROTOTYPES
#include "detail/chart_renderer.h"
#include "detail/display_theme.h"
#include "detail/gdal_support.h"
#include "detail/overlay_renderer.h"
#include "detail/render_primitives.h"
#include "detail/symbol_catalogue.h"
#include "viewer_impl.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <algorithm>
#include <cstdio>

namespace enc {
namespace detail {
Status graphicsAvailable() {
    const auto* version = glGetString(GL_VERSION);
    if (!version)
        return {ErrorCode::Graphics, "Se requiere un contexto OpenGL vigente.", {}};
    int major = 0, minor = 0;
    std::sscanf(reinterpret_cast<const char*>(version), "%d.%d", &major, &minor);
    if (major < 2 || (major == 2 && minor < 1))
        return {ErrorCode::Graphics, "Se requiere OpenGL de escritorio >=2.1.", {}};
    if (major >= 3) {
        GLint flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT)
            return {ErrorCode::Graphics,
                    "Un contexto forward-compatible no admite las primitivas del SDK.",
                    {}};
        if (major == 3 && minor == 1) {
            GLint count = 0;
            glGetIntegerv(GL_NUM_EXTENSIONS, &count);
            bool compatible = false;
            for (GLint i = 0; i < count; ++i) {
                const auto* extension = glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
                if (extension &&
                    std::string(reinterpret_cast<const char*>(extension)) == "GL_ARB_compatibility")
                    compatible = true;
            }
            if (!compatible)
                return {ErrorCode::Graphics, "OpenGL 3.1 requiere GL_ARB_compatibility.", {}};
        }
    }
    if (major > 3 || (major == 3 && minor >= 2)) {
        GLint profile = 0;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        if (!(profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT))
            return {ErrorCode::Graphics,
                    "El renderizador necesita perfil OpenGL de compatibilidad.",
                    {}};
    }
    return {};
}
} // namespace detail
namespace {
// El estado de atributos no incluye matrices, programa ni samplers: se conservan explícitamente.
class GraphicsGuard {
    GLint mode_ = GL_MODELVIEW, program_ = 0, active_ = GL_TEXTURE0, sampler_ = 0,
          unpackBuffer_ = 0;
    bool samplers_ = false;

  public:
    GraphicsGuard() {
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer_);
        glGetIntegerv(GL_MATRIX_MODE, &mode_);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active_);
        int major = 0, minor = 0;
        std::sscanf(reinterpret_cast<const char*>(glGetString(GL_VERSION)), "%d.%d", &major,
                    &minor);
        samplers_ = major > 3 || (major == 3 && minor >= 3);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
        glUseProgram(0);
        GLint units = 1;
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
        for (int i = 0; i < units; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glDisable(GL_TEXTURE_1D);
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_TEXTURE_3D);
            glDisable(GL_TEXTURE_CUBE_MAP);
            glDisable(GL_TEXTURE_GEN_S);
            glDisable(GL_TEXTURE_GEN_T);
            glDisable(GL_TEXTURE_GEN_R);
            glDisable(GL_TEXTURE_GEN_Q);
        }
        glActiveTexture(GL_TEXTURE0);
        if (samplers_) {
            glGetIntegeri_v(GL_SAMPLER_BINDING, 0, &sampler_);
            glBindSampler(0, 0);
        }
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        for (GLenum capability :
             {GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE, GL_LIGHTING, GL_FOG, GL_ALPHA_TEST,
              GL_COLOR_LOGIC_OP, GL_POLYGON_STIPPLE, GL_LINE_STIPPLE, GL_SCISSOR_TEST, GL_BLEND})
            glDisable(capability);
        GLint planes = 0;
        glGetIntegerv(GL_MAX_CLIP_PLANES, &planes);
        for (int i = 0; i < planes; ++i)
            glDisable(GL_CLIP_PLANE0 + i);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_FALSE);
        glBlendEquation(GL_FUNC_ADD);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        // Una PBO del anfitrión convertiría el puntero CPU de la textura en un desplazamiento.
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    ~GraphicsGuard() {
        glActiveTexture(GL_TEXTURE0);
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        if (samplers_)
            glBindSampler(0, static_cast<GLuint>(sampler_));
        glPopClientAttrib();
        glPopAttrib();
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpackBuffer_));
        glUseProgram(static_cast<GLuint>(program_));
        glActiveTexture(static_cast<GLenum>(active_));
        glMatrixMode(static_cast<GLenum>(mode_));
    }
};
Status validateTarget(const RenderTarget& target) {
    if (target.x < 0 || target.y < 0 || target.width < 0 || target.height < 0)
        return detail::invalid("Región de renderizado negativa.");
    return {};
}
void beginFrame(detail::SceneState& s, const RenderTarget& target) {
    s.framebufferWidth = target.width;
    s.framebufferHeight = target.height;
    s.viewportX = target.x;
    s.viewportY = target.y;
    glViewport(target.x, target.y, target.width, target.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, s.width, s.height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    s.gpu->collect();
}
} // namespace
Status Viewer::render(const RenderTarget& target) {
    if (auto valid = validateTarget(target); !valid)
        return valid;
    auto& s = impl_->scene;
    if (!target.width || !target.height || !s.width || !s.height)
        return {};
    if (auto available = detail::graphicsAvailable(); !available)
        return available;
    if (impl_->graphicsThread != std::thread::id{} &&
        impl_->graphicsThread != std::this_thread::get_id())
        return {ErrorCode::Graphics,
                "Usar el hilo propietario o liberar los recursos antes de transferir el contexto.",
                {}};
    impl_->graphicsThread = std::this_thread::get_id();
    GraphicsGuard guard;
    const detail::display::ScopedTheme theme(s.nightMode);
    beginFrame(s, target);
    detail::drawChart(s);
    detail::drawOverlays(s);
    return {};
}
Status Viewer::renderSymbolCatalogue(const RenderTarget& target) {
    if (auto valid = validateTarget(target); !valid)
        return valid;
    auto& s = impl_->scene;
    if (!target.width || !target.height || !s.width || !s.height)
        return {};
    if (auto available = detail::graphicsAvailable(); !available)
        return available;
    if (impl_->graphicsThread != std::thread::id{} &&
        impl_->graphicsThread != std::this_thread::get_id())
        return {
            ErrorCode::Graphics, "El catálogo debe dibujarse en el hilo gráfico propietario.", {}};
    impl_->graphicsThread = std::this_thread::get_id();
    GraphicsGuard guard;
    const detail::display::ScopedTheme theme(s.nightMode);
    beginFrame(s, target);
    glEnable(GL_SCISSOR_TEST);
    glScissor(target.x, target.y, target.width, target.height);
    detail::rect(0, 0, s.width, s.height, 0xF8F7F1);
    detail::drawText(18, 18, "ENC SDK / SYMBOL CATALOGUE / SIMPLIFIED S-57", 0x26373C,
                     s.width - 36);
    const auto& examples = detail::symbolCatalogue();
    const int columns = std::max(1, std::min(6, s.width / 145));
    const int rows = (static_cast<int>(examples.size()) + columns - 1) / columns;
    const double width = static_cast<double>(s.width) / columns,
                 height = static_cast<double>(std::max(1, s.height - 48)) / rows;
    for (std::size_t i = 0; i < examples.size(); ++i) {
        const double x = (i % columns) * width, y = 48 + (i / columns) * height;
        detail::nautical::draw(examples[i].symbol, x + width * .5, y + height * .58,
                               std::min(1.0, height / 72));
        detail::drawText(x + 6, y + height - 12, examples[i].label, 0x34484D,
                         static_cast<int>(width) - 12);
    }
    return {};
}
Status Viewer::releaseGraphicsResources() {
    auto& s = impl_->scene;
    if (s.gpu->live.empty()) {
        impl_->graphicsThread = {};
        return {};
    }
    if (auto available = detail::graphicsAvailable(); !available)
        return available;
    if (impl_->graphicsThread != std::thread::id{} &&
        impl_->graphicsThread != std::this_thread::get_id())
        return {ErrorCode::Graphics, "Liberar recursos en el hilo que los creó.", {}};
    for (auto& layer : s.rasters)
        layer.image.texture.reset();
    s.gpu->release();
    impl_->graphicsThread = {};
    return {};
}
} // namespace enc
