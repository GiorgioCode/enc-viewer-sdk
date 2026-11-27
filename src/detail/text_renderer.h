// text_renderer.h — Texto UTF-8 medido en píxeles lógicos con un atlas incorporado.
// Conserva las texturas dentro de la instancia y usa sustitución visible para glifos no incluidos.
#pragma once
#include "app_state.h"
namespace enc::detail {
double textWidth(const std::string& text, double scale = 1);
void drawLabelText(SceneState& scene, double x, double y, const std::string& text, unsigned color,
                   double scale = 1);
std::vector<std::uint32_t> decodeUtf8(const std::string& text);
} // namespace enc::detail
