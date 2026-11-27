// contact_renderer.h — Presentación de contactos AIS, radar y buque propio sobre la escena geográfica.
// El dibujo usa símbolos de la circular OMI SN.1/Circ.243/Rev.2 y conserva el color semántico de peligro.
#pragma once
#include "app_state.h"
namespace enc::detail {
void drawContacts(SceneState& scene);
}
