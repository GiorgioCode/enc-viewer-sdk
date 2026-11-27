// symbol_catalogue.h — Ejemplos de símbolos para el catálogo visual.
// Construye casos con el mismo decodificador y los mismos atributos que utilizan las cartas reales.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "chart_types.h"
#include "nautical_symbols.h"
#include <string>
#include <vector>

namespace enc::detail {

// Cada ejemplo se produce con los mismos códigos que una ayuda real de la carta.
struct CatalogueEntry {
    std::string label;
    nautical::Symbol symbol;
    std::string objectClass;
    std::vector<Attribute> attributes;
};

// Construye los ejemplos de la leyenda con el mismo decodificador que usan las cartas.
const std::vector<CatalogueEntry>& symbolCatalogue();

} // namespace enc::detail
