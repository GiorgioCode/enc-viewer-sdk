// picking.h — Contrato privado de selección de objetos.
// Devuelve un índice de la escena o -1 si el punto no intersecta objetos visibles.
#pragma once
#include "app_state.h"
namespace enc::detail {
std::vector<std::size_t> hitTests(const SceneState& scene, double x, double y, double tolerance);
int hitTest(const SceneState& scene, double x, double y);
} // namespace enc::detail
