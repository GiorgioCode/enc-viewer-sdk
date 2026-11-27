// symbol_catalogue.cpp — Ejemplos de símbolos para el catálogo visual.
// Construye casos con el mismo decodificador y los mismos atributos que utilizan las cartas reales.
// Contiene la implementación de esta responsabilidad.

#include "symbol_catalogue.h"
#include "chart_types.h"

namespace enc::detail {

// Construye los ejemplos de la leyenda con el mismo decodificador que usan las cartas.
const std::vector<CatalogueEntry>& symbolCatalogue() {
    static const auto catalogue = [] {
        std::vector<CatalogueEntry> entries;
        const auto add = [&](const char* label, const char* name,
                             std::initializer_list<Attribute> attributes) {
            const auto symbol = nautical::describe(name, [&](const std::string& key) {
                for (const auto& a : attributes)
                    if (a.name == key)
                        return a.value;
                return std::string{};
            });
            entries.push_back({label, symbol, name, attributes});
        };
        add("CONICAL BUOY", "BOYLAT", {{"BOYSHP", "1"}, {"COLOUR", "3"}});
        add("CAN BUOY", "BOYLAT", {{"BOYSHP", "2"}, {"COLOUR", "4"}});
        add("SPHERICAL BUOY", "BOYSAW", {{"BOYSHP", "3"}});
        add("PILLAR BUOY", "BOYLAT", {{"BOYSHP", "4"}, {"COLOUR", "3"}});
        add("SPAR BUOY", "BOYLAT", {{"BOYSHP", "5"}, {"COLOUR", "4"}});
        add("BARREL BUOY", "BOYSPP", {{"BOYSHP", "6"}});
        add("SUPER BUOY", "BOYSPP", {{"BOYSHP", "7"}});
        add("SPECIAL MARK", "BOYSPP", {{"BOYSHP", "4"}});
        add("CARDINAL NORTH", "BOYCAR", {{"BOYSHP", "4"}, {"CATCAM", "1"}});
        add("CARDINAL EAST", "BOYCAR", {{"BOYSHP", "4"}, {"CATCAM", "2"}});
        add("CARDINAL SOUTH", "BOYCAR", {{"BOYSHP", "4"}, {"CATCAM", "3"}});
        add("CARDINAL WEST", "BOYCAR", {{"BOYSHP", "4"}, {"CATCAM", "4"}});
        add("ISOLATED DANGER", "BOYISD", {{"BOYSHP", "4"}});
        add("SAFE WATER", "BOYSAW", {{"BOYSHP", "4"}});
        add("BEACON / TOWER", "BCNLAT", {{"BCNSHP", "3"}, {"COLOUR", "3"}, {"TOPSHP", "1"}});
        add("LIGHT / FLARE", "LIGHTS", {{"COLOUR", "6"}});
        add("WRECK", "WRECKS", {});
        add("UNDERWATER ROCK", "UWTROC", {});
        add("OBSTRUCTION", "OBSTRN", {});
        add("ANCHORAGE", "ACHARE", {});
        add("MOORING", "MORFAC", {});
        add("PILOT BOARDING", "PILBOP", {});
        add("MARINA", "SMCFAC", {});
        add("LANDMARK / TOWER", "LNDMRK", {{"CATLMK", "17"}});
        add("BUILDING", "BUISGL", {});
        add("SILO / TANK", "SILTNK", {});
        add("CRANE", "CRANES", {});
        add("PYLON / PILE", "PYLONS", {});
        add("BRIDGE", "BRIDGE", {});
        add("FOG SIGNAL", "FOGSIG", {});
        add("RADAR BEACON", "RTPBCN", {});
        add("OFFSHORE PLATFORM", "OFSPLF", {});
        add("WIND TURBINE", "LNDMRK", {{"CATLMK", "19"}});
        add("CHIMNEY", "LNDMRK", {{"CATLMK", "3"}});
        add("DAYMARK / TOPMARK", "TOPMAR", {{"TOPSHP", "12"}, {"COLOUR", "3"}});
        add("ROCK / EXPOSED", "UWTROC", {{"WATLEV", "2"}});
        add("ROCK / AWASH", "UWTROC", {{"WATLEV", "5"}});
        add("BUOY / CHECKERED", "BOYSPP", {{"BOYSHP", "7"}, {"COLOUR", "3,1"}, {"COLPAT", "4"}});
        add("BUOY / DIAGONAL", "BOYLAT", {{"BOYSHP", "4"}, {"COLOUR", "3,1"}, {"COLPAT", "3"}});
        add("BEACON / SAFE WATER", "BCNSAW", {{"BCNSHP", "3"}});
        add("BEACON / DANGER", "BCNISD", {{"BCNSHP", "3"}});
        add("BEACON / NORTH", "BCNCAR", {{"BCNSHP", "3"}, {"CATCAM", "1"}});
        add("LIGHT / SECTOR", "LIGHTS", {{"COLOUR", "3"}, {"SECTR1", "300"}, {"SECTR2", "40"}});
        add("OTHER / UNKNOWN", "UNSPECIFIED", {});
        return entries;
    }();
    return catalogue;
}

} // namespace enc::detail
