// chart_style.h — Reglas de presentación de objetos S-57.
// Asigna prioridades y colores, filtra por escala y combina luces y marcas con sus ayudas náuticas.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "app_state.h"

namespace enc::detail {

// El relleno y la prioridad pertenecen a la presentación, no a los atributos S-57.
struct Style {
    unsigned int fill = 0xB9D7D8, line = 0x304B50, point = 0xA13D32;
    int fillPass = -1; // Las áreas restantes sólo tienen contorno y no tapan el mapa base.
};

// Obtiene un atributo sin confundir un valor nulo con un nombre o una etiqueta real.
std::string attr(const Feature& feature, const std::string& name);

// Decodifica la simbología S-57 y asocia luces y marcas de tope dentro de cada celda.
void prepareSymbols(SceneState& app);

// Asigna relleno, contorno y prioridad de pintado a cada clase de objeto S-57.
Style styleFor(const SceneState& app, const Feature& feature);

// Identifica ayudas a la navegación que tienen prioridad visual y de selección.
bool isNavaid(nautical::Kind kind);

// Respeta capas, escala mínima y asociaciones para no duplicar equipos de una baliza.
bool pointVisible(const SceneState& app, const Feature& f, bool selected = false);

// Combina el cuerpo del símbolo con sus equipos únicamente si sus capas están activas.
nautical::Symbol visibleSymbol(const SceneState& app, const Feature& f);

} // namespace enc::detail
