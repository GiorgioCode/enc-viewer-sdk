// chart_reader.h — Lectores internos de datos geográficos.
// No forman parte de la API pública y no requieren una ventana.
#pragma once
#include "app_state.h"
namespace enc::detail {
bool loadBasemap(SceneState& scene, const std::string& source);
bool addCell(SceneState& scene, const fs::path& path);
} // namespace enc::detail
