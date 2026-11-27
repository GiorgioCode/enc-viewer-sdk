// symbols.h — Inspección pública de la simbología vectorial náutica incluida en el SDK.
// Permite consultar cómo se interpreta un objeto S-57 y obtener ejemplos sin depender de OpenGL.
#pragma once
#include "types.h"

namespace enc {
enum class SymbolKind {
    Unknown,
    Buoy,
    Beacon,
    Topmark,
    Light,
    Sounding,
    Rock,
    Wreck,
    Obstruction,
    Anchorage,
    Mooring,
    Pilot,
    Marina,
    Landmark,
    Building,
    Tank,
    Crane,
    Pylon,
    Bridge,
    FogSignal,
    Radar,
    WindTurbine,
    Platform
};
struct SymbolDescription {
    SymbolKind kind = SymbolKind::Unknown;
    std::string family;
    int shape = 0, category = 0, topShape = 0, colourPattern = 0, waterLevel = 0;
    std::vector<unsigned> colours;
    bool recognized = false;
};
struct SymbolExample {
    std::string label, objectClass;
    std::vector<Attribute> attributes;
    SymbolDescription symbol;
};
// Los valores desconocidos conservan una representación genérica explícita.
SymbolDescription describeSymbol(const std::string& objectClass,
                                 const std::vector<Attribute>& attributes);
const std::vector<SymbolExample>& symbolExamples();
} // namespace enc
