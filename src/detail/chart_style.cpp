// chart_style.cpp — Reglas de presentación de objetos S-57.
// Asigna prioridades y colores, filtra por escala y combina luces y marcas con sus ayudas náuticas.
// Contiene la implementación de esta responsabilidad.

#include "chart_style.h"
#include "camera.h"
#include "mosaic.h"
#include <cstdlib>
#include <unordered_map>

namespace enc::detail {

// Obtiene un atributo sin confundir un valor nulo con un nombre o una etiqueta real.
std::string attr(const Feature& feature, const std::string& name) {
    for (const auto& item : feature.attributes)
        if (item.name == name)
            return item.value == "<NULL>" ? "" : item.value;
    return {};
}

// Decodifica la simbología S-57 y asocia luces y marcas de tope dentro de cada celda.
void prepareSymbols(SceneState& app) {
    std::unordered_map<std::string, std::size_t> structures;
    const auto positionKey = [&app](const Feature& f) {
        if (f.points.empty())
            return std::string{};
        const auto& p = f.points.front();
        // El equipo se asocia únicamente a una estructura de su misma celda ENC.
        return app.layers[f.layer].cellKey + ":" + std::to_string(std::llround(p.x * 1e7)) + ":" +
               std::to_string(std::llround(p.y * 1e7));
    };
    std::unordered_map<std::string, std::size_t> identities;
    for (std::size_t i = 0; i < app.features.size(); ++i) {
        auto& f = app.features[i];
        f.symbol = nautical::describe(app.layers[f.layer].objectClass,
                                      [&f](const std::string& key) { return attr(f, key); });
        f.minScale = std::atof(attr(f, "SCAMIN").c_str());
        f.maxScale = std::atof(attr(f, "SCAMAX").c_str());
        if ((f.symbol.kind == nautical::Kind::Buoy || f.symbol.kind == nautical::Kind::Beacon) &&
            !f.points.empty()) {
            structures[positionKey(f)] = i;
            const auto identity = attr(f, "LNAM");
            if (!identity.empty())
                identities[app.layers[f.layer].cellKey + ":" + identity] = i;
        }
    }
    for (std::size_t i = 0; i < app.features.size(); ++i) {
        auto& f = app.features[i];
        if (f.symbol.kind != nautical::Kind::Light && f.symbol.kind != nautical::Kind::Topmark)
            continue;
        std::size_t parentIndex = app.features.size();
        const auto references = attr(f, "LNAM_REFS");
        // Las referencias explícitas se consultan por identidad, sin recorrer todas las ayudas.
        std::size_t start = 0;
        while (start < references.size()) {
            const auto end = references.find(',', start);
            const auto identity = identities.find(
                app.layers[f.layer].cellKey + ":" +
                references.substr(start, end == std::string::npos ? end : end - start));
            if (identity != identities.end()) {
                parentIndex = identity->second;
                break;
            }
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
        if (parentIndex == app.features.size()) {
            const auto parent = structures.find(positionKey(f));
            if (parent == structures.end())
                continue;
            parentIndex = parent->second;
        }
        f.parent = static_cast<int>(parentIndex);
        auto& structure = app.features[parentIndex];
        if (f.symbol.kind == nautical::Kind::Topmark)
            structure.topmark = static_cast<int>(i);
        else
            structure.light = static_cast<int>(i);
    }
}

// Asigna relleno, contorno y prioridad de pintado a cada clase de objeto S-57.
Style styleFor(const SceneState& app, const Feature& feature) {
    Style style;
    const std::string& name = app.layers[feature.layer].objectClass;
    if (name == "LNDARE" || name == "BUAARE") {
        style.fill = 0xE2D3A6;
        style.line = 0x725D3D;
        style.fillPass = name == "LNDARE" ? 1 : 3;
    } else if (name == "DEPARE" || name == "DRGARE") {
        const double depth =
            attr(feature, "DRVAL1").empty() ? 10 : std::atof(attr(feature, "DRVAL1").c_str());
        style.fill = depth < app.presentation.shallowDepth  ? 0x8EC2CE
                     : depth < app.presentation.safetyDepth ? 0xA8CFD5
                     : depth < app.presentation.deepDepth   ? 0xB8D9DB
                                                            : 0xC8E1DF;
        style.line = 0x6B969A;
        style.fillPass = 0;
    } else if (name == "LAKARE" || name == "RIVERS" || name == "DOCARE") {
        style.fillPass = 2;
    } else if (name.find("DEPCNT") != std::string::npos)
        style.line = 0x668D98;
    else if (name.find("COALNE") != std::string::npos)
        style.line = style.point = 0x50493C;
    else if (name.find("LIGHTS") != std::string::npos || name.find("BOY") != std::string::npos ||
             name.find("BCN") != std::string::npos)
        style.line = style.point = 0xB34635;
    else if (name.find("WRECK") != std::string::npos || name.find("OBSTRN") != std::string::npos)
        style.line = style.point = 0x8B3C32;
    if (const auto* rule = ruleFor(app, name)) {
        if (rule->fill) {
            style.fill = *rule->fill;
            if (style.fillPass < 0)
                style.fillPass = 3;
        }
        if (rule->line)
            style.line = style.point = *rule->line;
    }
    return style;
}

// Identifica ayudas a la navegación que tienen prioridad visual y de selección.
bool isNavaid(nautical::Kind kind) {
    return kind == nautical::Kind::Buoy || kind == nautical::Kind::Beacon ||
           kind == nautical::Kind::Light || kind == nautical::Kind::Topmark;
}

// Respeta capas, escala mínima y asociaciones para no duplicar equipos de una baliza.
bool pointVisible(const SceneState& app, const Feature& f, bool selected) {
    if (!featureVisible(app, f, selected))
        return false;
    if (selected)
        return true;
    if (f.parent >= 0 && app.layers[app.features[static_cast<std::size_t>(f.parent)].layer].visible)
        return false;
    return true;
}

// Combina el cuerpo del símbolo con sus equipos únicamente si sus capas están activas.
nautical::Symbol visibleSymbol(const SceneState& app, const Feature& f) {
    auto symbol = f.symbol;
    if (f.topmark >= 0) {
        const auto& top = app.features[static_cast<std::size_t>(f.topmark)];
        if (app.layers[top.layer].visible) {
            symbol.topShape = top.symbol.topShape;
            symbol.topColour = top.symbol.topColour;
        }
    }
    if (f.light >= 0) {
        const auto& light = app.features[static_cast<std::size_t>(f.light)];
        symbol.lighted = app.layers[light.layer].visible;
        symbol.lightColour = light.symbol.lightColour;
        symbol.sectorStart = light.symbol.sectorStart;
        symbol.sectorEnd = light.symbol.sectorEnd;
    }
    return symbol;
}

} // namespace enc::detail
