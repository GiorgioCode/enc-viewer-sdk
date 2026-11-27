// radar_scene.h — Construcción de la escena de radar a partir de las cartas.
// Elige un origen sobre agua, muestrea costas y estructuras y ubica los blancos sintéticos.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "app_state.h"

namespace enc::detail {

// Busca el centro de un triángulo de profundidad cercano al punto pedido.
overlays::GeoPoint nearestWater(const SceneState& app, overlays::GeoPoint desired);

// Muestrea costa y estructuras de la carta y ubica los blancos sintéticos del radar.
void buildRadarScene(SceneState& app);

// Selecciona un origen inicial sobre agua y un alcance adecuado a la vista actual.
void initializeRadar(SceneState& app);

// Cambia el alcance y vuelve a muestrear los ecos que quedan dentro del círculo.
void setRadarRange(SceneState& app, int direction);

} // namespace enc::detail
