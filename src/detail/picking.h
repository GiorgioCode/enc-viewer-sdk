// picking.h — Contrato privado de selección de objetos.
// Devuelve un índice de la escena o -1 si el punto no intersecta objetos visibles.
#pragma once
#include "app_state.h"
namespace enc::detail {
int hitTest(const SceneState& scene, double x, double y);
}
