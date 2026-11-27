// symbols.cpp — Adaptación de los símbolos privados a descripciones públicas sin OpenGL.
// Comparte el decodificador de las cartas con el catálogo de ejemplos para evitar interpretaciones distintas.
#include "detail/nautical_symbols.h"
#include "detail/symbol_catalogue.h"
#include <enc/symbols.h>
namespace enc {
SymbolDescription describeSymbol(const std::string& objectClass,
                                 const std::vector<Attribute>& attributes) {
    const auto symbol = detail::nautical::describe(objectClass, [&](const std::string& name) {
        for (const auto& a : attributes)
            if (a.name == name)
                return a.value == "<NULL>" ? std::string{} : a.value;
        return std::string{};
    });
    const char* names[] = {"Desconocido",
                           "Boya",
                           "Baliza",
                           "Marca de tope",
                           "Luz",
                           "Sonda",
                           "Roca",
                           "Naufragio",
                           "Obstrucción",
                           "Fondeadero",
                           "Amarre",
                           "Práctico",
                           "Puerto deportivo",
                           "Marca terrestre",
                           "Edificio",
                           "Tanque",
                           "Grúa",
                           "Pilón",
                           "Puente",
                           "Señal de niebla",
                           "Radar",
                           "Aerogenerador",
                           "Plataforma"};
    return {symbol.kind,
            names[static_cast<std::size_t>(symbol.kind)],
            symbol.shape,
            symbol.category,
            symbol.topShape,
            symbol.pattern,
            symbol.waterLevel,
            symbol.colours,
            symbol.kind != SymbolKind::Unknown};
}
const std::vector<SymbolExample>& symbolExamples() {
    static const auto examples = [] {
        std::vector<SymbolExample> result;
        for (const auto& e : detail::symbolCatalogue()) {
            SymbolExample example;
            example.label = e.label;
            example.objectClass = e.objectClass;
            for (const auto& a : e.attributes)
                example.attributes.push_back({a.name, a.value});
            example.symbol = describeSymbol(example.objectClass, example.attributes);
            result.push_back(std::move(example));
        }
        return result;
    }();
    return examples;
}
} // namespace enc
