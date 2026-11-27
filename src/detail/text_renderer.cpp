// text_renderer.cpp — Decodificación UTF-8, medición y dibujo de etiquetas con textura alfa.
// El atlas cubre latín extendido, griego, cirílico y puntuación; no necesita fuentes instaladas.
#include "text_renderer.h"
#include "font_data.h"
#include "render_primitives.h"
#include <algorithm>
namespace enc::detail {
namespace {
constexpr int glyphs = 1297, width = 640, height = 984;
int glyphIndex(std::uint32_t c) {
    if (c >= 32 && c < 768)
        return c - 32;
    if (c >= 880 && c < 1328)
        return 736 + c - 880;
    if (c >= 8192 && c < 8304)
        return 1184 + c - 8192;
    return 1296;
}
void prepare(SceneState& s) {
    auto& atlas = s.fontAtlas;
    if (!atlas.loaded()) {
        atlas.width = width;
        atlas.height = height;
        atlas.rgba.resize(width * height * 4, 255);
        std::size_t pixel = 0;
        for (std::size_t i = glyphs; i + 1 < sizeof(kEmbeddedFont); i += 2)
            for (int n = 0; n < kEmbeddedFont[i] && pixel < width * height; ++n, ++pixel)
                atlas.rgba[pixel * 4 + 3] = kEmbeddedFont[i + 1];
    }
    atlas.upload(s.gpu);
}
} // namespace
std::vector<std::uint32_t> decodeUtf8(const std::string& text) {
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        int n = c < 128                  ? 1
                : c >= 0xc2 && c <= 0xdf ? 2
                : c >= 0xe0 && c <= 0xef ? 3
                : c >= 0xf0 && c <= 0xf4 ? 4
                                         : 0;
        std::uint32_t code = n == 1 ? c : n == 2 ? c & 31 : n == 3 ? c & 15 : c & 7;
        bool valid = n > 0 && i + n <= text.size();
        for (int k = 1; valid && k < n; ++k) {
            const auto next = static_cast<unsigned char>(text[i + k]);
            valid = (next & 0xc0) == 0x80;
            code = (code << 6) | (next & 63);
        }
        valid = valid && code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff) &&
                !(n == 2 && code < 128) && !(n == 3 && code < 2048) && !(n == 4 && code < 65536);
        out.push_back(valid ? code : 0xfffd);
        i += valid ? n : 1;
    }
    return out;
}
double textWidth(const std::string& text, double scale) {
    double sum = 0;
    for (auto c : decodeUtf8(text))
        sum += kEmbeddedFont[glyphIndex(c)];
    return sum * scale;
}
void drawLabelText(SceneState& s, double x, double y, const std::string& text, unsigned colour,
                   double scale) {
    prepare(s);
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s.fontAtlas.texture->id);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    setHex(colour);
    glBegin(GL_QUADS);
    for (auto code : decodeUtf8(text)) {
        const int index = glyphIndex(code);
        const double u = (index % 32) * 20. / width, v = (index / 32) * 24. / height;
        glTexCoord2d(u, v);
        glVertex2d(x, y);
        glTexCoord2d(u + 20. / width, v);
        glVertex2d(x + 20 * scale, y);
        glTexCoord2d(u + 20. / width, v + 24. / height);
        glVertex2d(x + 20 * scale, y + 24 * scale);
        glTexCoord2d(u, v + 24. / height);
        glVertex2d(x, y + 24 * scale);
        x += kEmbeddedFont[index] * scale;
    }
    glEnd();
    glPopAttrib();
}
} // namespace enc::detail
