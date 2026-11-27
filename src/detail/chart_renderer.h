// chart_renderer.h — Renderizado del mundo y las cartas ENC.
// Compone cobertura, superficies, líneas, símbolos, selección y etiquetas sin colisiones.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "app_state.h"

namespace enc::detail {

// Pinta mundo, cobertura, superficies, líneas y símbolos en el orden de superposición.
void drawChart(SceneState& app);

} // namespace enc::detail
