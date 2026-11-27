// nautical_symbols.h — Catálogo vectorial y decodificación de simbología náutica.
// Interpreta códigos S-57 y dibuja cuerpos, marcas de tope, luces y otras familias de objetos.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

// Catálogo vectorial propio inspirado en NOAA Chart No. 1. Los atributos siguen
// el catálogo S-57 distribuido con GDAL. Representa un subconjunto visual;
// no implementa la biblioteca de presentación IHO S-52 completa.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <enc/symbols.h>
#include <limits>
#include <string>
#include <vector>

namespace enc::detail::nautical {
constexpr unsigned ink = 0x25383C;
constexpr unsigned red = 0xC43D38;
constexpr unsigned green = 0x258B59;
constexpr unsigned yellow = 0xF2CA43;
constexpr unsigned magenta = 0xAC438F;
using Kind = enc::SymbolKind;
// Descriptor independiente de GDAL: conserva códigos S-57 y colores base sin aplicar la paleta.
struct Symbol {
    Kind kind = Kind::Unknown;
    int shape = 0, category = 0, topShape = 0, pattern = 0;
    std::vector<unsigned> colours;
    unsigned topColour = ink, lightColour = yellow;
    bool lighted = false;
    int waterLevel = 0;
    double sectorStart = std::numeric_limits<double>::quiet_NaN();
    double sectorEnd = std::numeric_limits<double>::quiet_NaN();
};

// Clasifica un nombre de objeto S-57 en una familia del catálogo.
Kind kindFor(const std::string& name);
// Descompone listas de códigos usadas por colores, patrones y categorías.
std::vector<int> codes(const std::string& text);
// Obtiene el color base de un código COLOUR, antes de la paleta día/noche.
unsigned colour(int code);
// Dibuja un círculo de apoyo en píxeles lógicos.
void circle(double x, double y, double r, unsigned fill, bool solid = true);
// Renderiza un descriptor con posición y escala locales a la pantalla.
void draw(const Symbol& symbol, double x, double y, double scale = 1);

// El acceso por función permite reutilizar la decodificación para cartas y catálogo visual.
template <class Getter> Symbol describe(const std::string& name, Getter get) {
    Symbol symbol;
    symbol.kind = kindFor(name);
    symbol.waterLevel = std::atoi(get("WATLEV").c_str());
    const auto sector1 = get("SECTR1"), sector2 = get("SECTR2");
    if (!sector1.empty())
        symbol.sectorStart = std::atof(sector1.c_str());
    if (!sector2.empty())
        symbol.sectorEnd = std::atof(sector2.c_str());
    symbol.shape = std::atoi(get(symbol.kind == Kind::Beacon ? "BCNSHP" : "BOYSHP").c_str());
    symbol.topShape = std::atoi(get("TOPSHP").c_str());
    const auto patterns = codes(get("COLPAT"));
    if (!patterns.empty())
        symbol.pattern = patterns.front();
    for (int code : codes(get("COLOUR")))
        symbol.colours.push_back(colour(code));
    if (symbol.kind == Kind::Beacon && symbol.topShape && !symbol.colours.empty())
        symbol.topColour = symbol.colours.front();
    if (symbol.kind == Kind::Landmark) {
        const auto categories = codes(get("CATLMK"));
        if (!categories.empty())
            symbol.category = categories.front();
    }
    // Las marcas cardinales, de peligro, aguas seguras y especiales tienen reglas propias.
    if (name == "BOYCAR" || name == "BCNCAR") {
        symbol.topColour = ink;
        symbol.category = std::atoi(get("CATCAM").c_str());
        switch (symbol.category) {
        case 1:
            symbol.topShape = 13;
            if (symbol.colours.empty())
                symbol.colours = {ink, yellow};
            break;
        case 2:
            symbol.topShape = 11;
            if (symbol.colours.empty())
                symbol.colours = {ink, yellow, ink};
            break;
        case 3:
            symbol.topShape = 14;
            if (symbol.colours.empty())
                symbol.colours = {yellow, ink};
            break;
        case 4:
            symbol.topShape = 10;
            if (symbol.colours.empty())
                symbol.colours = {yellow, ink, yellow};
            break;
        }
        symbol.pattern = 1;
    } else if (name == "BOYISD" || name == "BCNISD") {
        symbol.topColour = ink;
        symbol.topShape = 4;
        if (symbol.colours.empty())
            symbol.colours = {ink, red, ink};
        symbol.pattern = 1;
    } else if (name == "BOYSAW" || name == "BCNSAW") {
        symbol.topShape = 3;
        symbol.topColour = red;
        if (symbol.colours.empty())
            symbol.colours = {red, 0xFFFFFF};
        symbol.pattern = 2;
    } else if (name == "BOYSPP" || name == "BCNSPP") {
        symbol.topShape = 7;
        symbol.topColour = yellow;
        if (symbol.colours.empty())
            symbol.colours = {yellow};
    }
    // Sin forma o color lateral se conserva una figura genérica: CATLAM por sí solo
    // no permite determinar si la fuente utiliza la región IALA A o B.
    if (symbol.colours.empty())
        symbol.colours = {ink};
    if (symbol.kind == Kind::Light)
        symbol.lightColour = symbol.colours.front();
    if (symbol.kind == Kind::Topmark)
        symbol.topColour = symbol.colours.front();
    return symbol;
}

} // namespace enc::detail::nautical
