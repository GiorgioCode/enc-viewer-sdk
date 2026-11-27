// interface.cpp — Paneles del ejemplo de integración, construidos con información de la API pública.
// Dibuja controles, listas, atributos y diálogo de rutas; el mapa pertenece enteramente a enc::Viewer.
#include "app.h"
#include "drawing.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
namespace demo {
std::vector<Button> buttons(const App& a) {
    std::vector<Button> result;
    const auto add = [&](double x, double y, double w, const std::string& label, Action action,
                         bool active = false) {
        result.push_back({x, y, w, 28, label, action, active});
    };
    add(12, 14, 128, "ABRIR CARTAS O", Action::Open);
    add(150, 14, 72, "MUNDO W", Action::World);
    add(232, 14, 72, "AJUSTAR F", Action::Fit);
    add(314, 14, 104, "CATALOGO L", Action::Catalogue, a.catalogue);
    add(a.width - 112, 14, 96, a.viewer.theme() == enc::Theme::Night ? "NOCHE N" : "DIA N",
        Action::Theme, true);
    const int x = a.mapWidth() + 16, y = a.overlayTop();
    add(x, 72, 202, a.viewer.chartsVisible() ? "CARTAS ENC V / ON" : "CARTAS ENC V / OFF",
        Action::Charts, a.viewer.chartsVisible());
    const auto radar = a.viewer.radarInfo().settings;
    add(x, y + 24, 102, "RADAR R", Action::Radar, radar.visible);
    add(x + 110, y + 24, 98, radar.paused ? "REANUDAR P" : "PAUSA P", Action::Pause, radar.paused);
    add(x + 216, y + 24, 100, "ORIGEN C", Action::Origin, a.placingRadar);
    add(x + 112, y + 57, 24, "-", Action::RangeDown);
    add(x + 144, y + 57, 24, "+", Action::RangeUp);
    add(x + 190, y + 57, 24, "-", Action::GainDown);
    add(x + 222, y + 57, 24, "+", Action::GainUp);
    add(x + 264, y + 57, 24, "-", Action::RadarDown);
    add(x + 296, y + 57, 24, "+", Action::RadarUp);
    add(x, y + 128, 100, "ABRIR TIFF T", Action::Tiff);
    add(x + 108, y + 128, 68, "DEMO", Action::Demo);
    const auto image = a.image();
    add(x + 184, y + 128, 132, image && image->visible ? "OCULTAR I" : "MOSTRAR I",
        Action::ImageVisible, image && image->visible);
    add(x + 100, y + 162, 24, "-", Action::ImageDown);
    add(x + 132, y + 162, 24, "+", Action::ImageUp);
    add(x + 174, y + 162, 142, "AJUSTAR IMAGEN K", Action::ImageFit);
    add(x, y + 196, 64, "< ANT", Action::Previous);
    add(x + 72, y + 196, 64, "SIG >", Action::Next);
    add(x + 144, y + 196, 172, "RESTAURAR J", Action::Reset);
    add(x, y + 230, 152, "QUITAR IMAGEN", Action::Remove);
    add(x + 164, y + 230, 152, "SUBIR AL TOPE", Action::ToTop);
    return result;
}
bool interfaceClick(App& a) {
    for (const auto& b : buttons(a))
        if (a.mouseX >= b.x && a.mouseX < b.x + b.width && a.mouseY >= b.y &&
            a.mouseY < b.y + b.height) {
            a.action(b.action);
            return true;
        }
    if (a.mouseX >= a.mapWidth() && a.mouseY >= 112 && a.mouseY < a.layerBottom()) {
        const auto index = static_cast<std::size_t>(
            std::max(0, a.layerScroll + static_cast<int>(a.mouseY - 112) / 18));
        const auto layers = a.viewer.layers();
        if (index < layers.size())
            a.report(a.viewer.setLayerVisible(index, !layers[index].visible));
        return true;
    }
    return !a.onMap(a.mouseX, a.mouseY);
}
void drawInterface(App& a) {
    const bool night = a.viewer.theme() == enc::Theme::Night;
    const unsigned panel = night ? 0x111A22 : 0xF3F0E6, ink = night ? 0xB8C5CF : 0x263D43;
    const unsigned muted = night ? 0x748B9B : 0x657879, button = night ? 0x203240 : 0xDDE4D9,
                   active = night ? 0x354C59 : 0xB5D0C0;
    glViewport(0, 0, a.frameWidth, a.frameHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, a.width, a.height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    rectangle(0, 0, a.width, toolbar, panel);
    rectangle(a.mapWidth(), toolbar, sidebar, a.height - toolbar, panel);
    rectangle(0, a.height - footer, a.width, footer, panel);
    for (const auto& b : buttons(a)) {
        rectangle(b.x, b.y, b.width, b.height, b.active ? active : button);
        text(b.x + 8, b.y + 10, b.label, ink, static_cast<int>(b.width) - 12);
    }
    text(434, 25, "ENC SDK / API PUBLICA", muted, a.mapWidth() - 450);
    const auto scene = a.viewer.sceneInfo();
    const int x = a.mapWidth() + 16;
    text(x, 60, "COLECCION ENC", ink);
    text(x + 216, 84, std::to_string(scene.cellCount) + " CELDAS", muted, 112);
    const auto layers = a.viewer.layers();
    const int rows = std::max(0, (a.layerBottom() - 112) / 18);
    a.layerScroll =
        std::clamp(a.layerScroll, 0, std::max(0, static_cast<int>(layers.size()) - rows));
    for (int row = 0; row < rows && row + a.layerScroll < static_cast<int>(layers.size()); ++row) {
        const auto& layer = layers[static_cast<std::size_t>(row + a.layerScroll)];
        const int y = 112 + row * 18;
        rectangle(x, y + 1, 9, 9, layer.visible ? active : button);
        text(x + 17, y + 2, layer.name, ink, 244);
        text(a.width - 54, y + 2, std::to_string(layer.featureCount), muted, 42);
    }
    const int detailTop = a.layerBottom() + 18, detailBottom = a.overlayTop() - 14;
    text(x, detailTop, "OBJETO SELECCIONADO", ink);
    const auto selected = a.viewer.selected();
    if (!selected)
        text(x, detailTop + 22, "CLIC EN UNA BOYA, LINEA O AREA", muted, 316);
    else {
        text(x, detailTop + 18, selected->objectClass + " / " + selected->symbol.family, ink, 316);
        const int visible = std::max(0, (detailBottom - detailTop - 36) / 15);
        a.attributeScroll =
            std::clamp(a.attributeScroll, 0,
                       std::max(0, static_cast<int>(selected->attributes.size()) - visible));
        for (int i = 0;
             i < visible && i + a.attributeScroll < static_cast<int>(selected->attributes.size());
             ++i) {
            const auto& attribute =
                selected->attributes[static_cast<std::size_t>(i + a.attributeScroll)];
            text(x, detailTop + 36 + i * 15, attribute.name, muted, 88);
            text(x + 96, detailTop + 36 + i * 15, attribute.value, ink, 220);
        }
    }
    const auto radar = a.viewer.radarInfo();
    const int y = a.overlayTop();
    text(x, y + 8, "CAPAS SUPERPUESTAS", ink);
    std::ostringstream range;
    range << std::fixed << std::setprecision(3) << radar.settings.rangeNm << " NM";
    text(x, y + 68, range.str(), muted, 108);
    text(x + 190, y + 92, "GAIN", muted);
    text(x + 264, y + 92, "ALFA", muted);
    const auto rasters = a.viewer.rasters();
    text(x, y + 112, "TIFF / " + std::to_string(rasters.size()) + " IMAGENES", ink, 316);
    const auto image = a.image();
    if (image) {
        text(x, y + 172, std::to_string(static_cast<int>(image->opacity * 100)) + "%", muted, 90);
        text(x, y + 268,
             (image->georeferenced ? "GEO / " : "MANUAL / ") + image->path.filename().string(),
             muted, 316);
    } else
        text(x, y + 268, "SIN IMAGEN / ABRIR O USAR DEMO", muted, 316);
    text(x, y + 287, "SHIFT + ARRASTRE / RUEDA: EDITAR TIFF", muted, 316);
    text(x, y + 303, "RUEDA SOBRE LISTAS: DESPLAZAR", muted, 316);
    std::string status = a.placingRadar ? "CLIC EN EL MAPA PARA UBICAR EL RADAR" : a.status;
    if (a.onMap(a.mouseX, a.mouseY)) {
        const auto p = a.viewer.screenToGeo(a.mapPoint(a.mouseX, a.mouseY));
        std::ostringstream coordinates;
        coordinates << std::fixed << std::setprecision(5) << "  LON " << p.lon << " LAT " << p.lat;
        status += coordinates.str();
    }
    text(12, a.height - 15, status, muted, a.width - 24);
    if (a.prompt != Prompt::None) {
        const int w = std::min(820, a.width - 80), left = (a.width - w) / 2,
                  top = a.height / 2 - 65;
        rectangle(left - 3, top - 3, w + 6, 136, active);
        rectangle(left, top, w, 130, panel);
        text(left + 18, top + 18,
             a.prompt == Prompt::Charts ? "CARPETA ENC O CELDA .000" : "ARCHIVO TIFF / GEOTIFF",
             ink, w - 36);
        rectangle(left + 16, top + 43, w - 32, 30, button);
        text(left + 24, top + 54, a.promptText, ink, w - 48);
        text(left + 18, top + 93, "ENTER: CARGAR / ESC: CANCELAR / CTRL+V: PEGAR", muted, w - 36);
    }
}
} // namespace demo
