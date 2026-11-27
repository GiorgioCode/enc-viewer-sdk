// app.cpp — Traducción de acciones de interfaz a llamadas públicas de la biblioteca.
// Gestiona índice, diagnósticos e imagen activa; no accede a las geometrías o texturas internas.
#include "app.h"
#include <algorithm>
#include <iostream>
namespace demo {
int App::layerBottom() const {
    return std::max(200, static_cast<int>(height * .31));
}
bool App::onMap(double x, double y) const {
    return x >= 0 && x < mapWidth() && y >= toolbar && y < height - footer;
}
void App::report(const enc::Status& result) {
    if (!result.message.empty())
        status = result.message;
    for (const auto& d : result.diagnostics)
        std::cerr << d.path << ": " << d.message << '\n';
    if (!result)
        std::cerr << result.message << '\n';
}
void App::loadCharts(const std::filesystem::path& source) {
    enc::Catalog candidate;
    report(candidate.scan(source));
    if (candidate.source().empty())
        return;
    const auto result = viewer.loadCatalog(candidate);
    report(result);
    if (result) {
        catalog = std::move(candidate);
        chartSource = source;
        layerScroll = attributeScroll = 0;
    }
}
void App::openImage(const std::filesystem::path& source) {
    const auto result = viewer.addGeoTiff(source);
    report(result.status);
    if (result)
        activeImage = result.value;
}
void App::demoImage() {
    const auto result = viewer.addDemoGeoTiff();
    report(result.status);
    if (result)
        activeImage = result.value;
}
void App::openPrompt(Prompt type) {
    prompt = type;
    promptText = type == Prompt::Charts ? chartSource.string() : "";
    dragging = false;
    mouseDown = false;
}
std::optional<enc::RasterInfo> App::image() const {
    for (const auto& r : viewer.rasters())
        if (activeImage && r.id == *activeImage)
            return r;
    return {};
}
void App::action(Action action) {
    auto radar = viewer.radarInfo().settings;
    const auto current = image();
    switch (action) {
    case Action::Open:
        openPrompt(Prompt::Charts);
        break;
    case Action::Fit:
        viewer.fitCharts();
        break;
    case Action::World:
        viewer.fitWorld();
        break;
    case Action::Catalogue:
        catalogue = !catalogue;
        break;
    case Action::Theme:
        viewer.setTheme(viewer.theme() == enc::Theme::Day ? enc::Theme::Night : enc::Theme::Day);
        break;
    case Action::Charts:
        viewer.setChartsVisible(!viewer.chartsVisible());
        break;
    case Action::Radar:
        viewer.enableRadar(!radar.visible);
        break;
    case Action::Pause:
        radar.paused = !radar.paused;
        report(viewer.configureRadar(radar));
        break;
    case Action::Origin:
        viewer.enableRadar(true);
        placingRadar = !placingRadar;
        break;
    case Action::RangeDown:
        report(viewer.changeRadarRange(-1));
        break;
    case Action::RangeUp:
        report(viewer.changeRadarRange(1));
        break;
    case Action::GainDown:
        radar.gain = std::max(0.0, radar.gain - .1);
        report(viewer.configureRadar(radar));
        break;
    case Action::GainUp:
        radar.gain = std::min(10.0, radar.gain + .1);
        report(viewer.configureRadar(radar));
        break;
    case Action::RadarDown:
        radar.opacity = std::max(0.0, radar.opacity - .05);
        report(viewer.configureRadar(radar));
        break;
    case Action::RadarUp:
        radar.opacity = std::min(1.0, radar.opacity + .05);
        report(viewer.configureRadar(radar));
        break;
    case Action::Tiff:
        openPrompt(Prompt::Tiff);
        break;
    case Action::Demo:
        demoImage();
        break;
    case Action::ImageVisible:
        if (current)
            report(viewer.setRasterVisible(current->id, !current->visible));
        break;
    case Action::ImageDown:
        if (current)
            report(viewer.setRasterOpacity(current->id, std::max(0.0, current->opacity - .05)));
        break;
    case Action::ImageUp:
        if (current)
            report(viewer.setRasterOpacity(current->id, std::min(1.0, current->opacity + .05)));
        break;
    case Action::ImageFit:
        if (current)
            report(viewer.fitRaster(current->id));
        break;
    case Action::Reset:
        if (current)
            report(viewer.resetRaster(current->id));
        break;
    case Action::Remove:
        if (current)
            report(viewer.removeRaster(current->id));
        if (const auto list = viewer.rasters(); !list.empty())
            activeImage = list.back().id;
        else
            activeImage.reset();
        break;
    case Action::ToTop:
        if (current)
            report(viewer.moveRasterToTop(current->id));
        break;
    case Action::Previous:
    case Action::Next: {
        const auto list = viewer.rasters();
        if (list.empty())
            break;
        std::size_t index = 0;
        while (index < list.size() && (!activeImage || list[index].id != *activeImage))
            ++index;
        if (index == list.size())
            index = 0;
        index = (index + list.size() + (action == Action::Previous ? -1 : 1)) % list.size();
        activeImage = list[index].id;
        break;
    }
    }
}
} // namespace demo
