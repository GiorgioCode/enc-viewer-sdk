// portrayal.h — Reglas de presentación y descripciones legibles independientes del renderizador.
// Permite adaptar colores, categorías y prioridades sin exponer tipos GDAL u OpenGL.
#pragma once
#include "types.h"
#include <optional>
namespace enc {
enum class DisplayCategory { Base, Standard, Other };
enum class LinePattern { Solid, Dashed, Dotted };
struct PortrayalRule {
    std::string objectClass;
    DisplayCategory category = DisplayCategory::Other;
    int priority = 5;
    std::optional<unsigned> fill, line;
    float lineWidth = 1;
    LinePattern pattern = LinePattern::Solid;
};
// Las reglas propias son un subconjunto configurable, no una biblioteca S-52 completa.
std::vector<PortrayalRule> defaultPortrayalRules();
std::string objectClassDescription(const std::string& acronym);
std::string attributeDescription(const std::string& acronym);
struct AttributeDescription {
    std::string acronym, name, value, meaning;
};
std::vector<AttributeDescription> describeAttributes(const std::vector<Attribute>& attributes);
} // namespace enc
