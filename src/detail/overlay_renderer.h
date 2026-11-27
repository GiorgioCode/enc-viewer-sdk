// overlay_renderer.h — Composición gráfica de las superposiciones.
// Dibuja primero el TIFF y después el PPI, recortados al mapa y adaptados al modo día o noche.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "app_state.h"

namespace enc::detail {

// Recorta las superposiciones al mapa y compone primero el TIFF y después el PPI.
void drawOverlays(SceneState& app);

} // namespace enc::detail
