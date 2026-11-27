// input.cpp — Adaptador GLFW de mouse, teclado y archivos arrastrados al ejemplo.
// Convierte coordenadas de ventana a coordenadas locales del mapa y llama únicamente a la API pública.
#include "app.h"
#include <algorithm>
#include <cctype>
#include <cmath>
namespace demo {
namespace {
App& state(GLFWwindow* window) {
    return *static_cast<App*>(glfwGetWindowUserPointer(window));
}
bool tiff(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".tif" || ext == ".tiff";
}
void character(GLFWwindow* window, unsigned codepoint) {
    auto& a = state(window);
    if (a.suppressCharacter) {
        a.suppressCharacter = false;
        return;
    }
    if (a.prompt == Prompt::None || codepoint < 32 || codepoint > 0x10FFFF ||
        a.promptText.size() > 4092)
        return;
    if (codepoint < 128)
        a.promptText.push_back(static_cast<char>(codepoint));
    else {
        const int count = codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
        a.promptText.push_back(static_cast<char>((count == 2   ? 0xC0
                                                  : count == 3 ? 0xE0
                                                               : 0xF0) |
                                                 (codepoint >> (6 * (count - 1)))));
        for (int i = count - 2; i >= 0; --i)
            a.promptText.push_back(static_cast<char>(0x80 | ((codepoint >> (6 * i)) & 0x3F)));
    }
}
void key(GLFWwindow* window, int key, int, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;
    auto& a = state(window);
    if (a.prompt != Prompt::None) {
        if (key == GLFW_KEY_ESCAPE)
            a.prompt = Prompt::None;
        else if (key == GLFW_KEY_BACKSPACE && !a.promptText.empty()) {
            std::size_t i = a.promptText.size() - 1;
            while (i > 0 && (static_cast<unsigned char>(a.promptText[i]) & 0xC0) == 0x80)
                --i;
            a.promptText.erase(i);
        } else if (key == GLFW_KEY_V && (mods & GLFW_MOD_CONTROL)) {
            if (const char* value = glfwGetClipboardString(window))
                a.promptText += std::string(value).substr(0, 4096 - a.promptText.size());
        } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            auto path = a.promptText;
            const auto kind = a.prompt;
            a.prompt = Prompt::None;
            if (path.size() > 1 && ((path.front() == '"' && path.back() == '"') ||
                                    (path.front() == '\'' && path.back() == '\'')))
                path = path.substr(1, path.size() - 2);
            if (!path.empty()) {
                if (kind == Prompt::Charts)
                    a.loadCharts(path);
                else
                    a.openImage(path);
            }
        }
        return;
    }
    // Las acciones de alternancia no se repiten al mantener presionada la tecla.
    if (action != GLFW_PRESS)
        return;
    switch (key) {
    case GLFW_KEY_ESCAPE:
        if (a.placingRadar)
            a.placingRadar = false;
        else if (a.catalogue)
            a.catalogue = false;
        else
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    case GLFW_KEY_Q:
    case GLFW_KEY_E: {
        a.viewer.rememberView();
        auto camera = a.viewer.camera();
        camera.rotationDegrees += key == GLFW_KEY_Q ? -15 : 15;
        a.report(a.viewer.setCamera(camera));
        break;
    }
    case GLFW_KEY_M: {
        a.viewer.rememberView();
        auto camera = a.viewer.camera();
        camera.projection = camera.projection == enc::Projection::Mercator
                                ? enc::Projection::LocalEquirectangular
                                : enc::Projection::Mercator;
        a.report(a.viewer.setCamera(camera));
        break;
    }
    case GLFW_KEY_H:
        a.viewer.previousView();
        break;
    case GLFW_KEY_Y:
        a.viewer.nextView();
        break;
    case GLFW_KEY_D: {
        auto p = a.viewer.presentation();
        p.displayMode = p.displayMode == enc::DisplayMode::All    ? enc::DisplayMode::Base
                        : p.displayMode == enc::DisplayMode::Base ? enc::DisplayMode::Standard
                                                                  : enc::DisplayMode::All;
        a.report(a.viewer.setPresentation(p));
        break;
    }
    case GLFW_KEY_B: {
        auto settings = a.viewer.contactSettings();
        settings.visible = !settings.visible;
        a.report(a.viewer.configureContacts(settings));
        break;
    }
    case GLFW_KEY_S: {
        auto settings = a.viewer.contactSettings();
        settings.showSleeping = !settings.showSleeping;
        a.report(a.viewer.configureContacts(settings));
        break;
    }
    case GLFW_KEY_U: {
        auto settings = a.viewer.contactSettings();
        settings.vectors = settings.vectors == enc::VectorMode::True ? enc::VectorMode::Relative
                                                                     : enc::VectorMode::True;
        a.report(a.viewer.configureContacts(settings));
        break;
    }
    case GLFW_KEY_A: {
        auto settings = a.viewer.contactSettings();
        settings.associatedAsAIS = !settings.associatedAsAIS;
        a.report(a.viewer.configureContacts(settings));
        break;
    }
    case GLFW_KEY_G:
        if (a.selectedContactId)
            a.report(a.viewer.acknowledgeContact(*a.selectedContactId));
        break;
    case GLFW_KEY_F8:
    case GLFW_KEY_F9: {
        auto p = a.viewer.presentation();
        p.textScale = std::clamp(p.textScale + (key == GLFW_KEY_F9 ? .1 : -.1), .5, 3.);
        a.report(a.viewer.setPresentation(p));
        break;
    }
    case GLFW_KEY_F6: {
        auto p = a.viewer.presentation();
        p.preferNationalNames = !p.preferNationalNames;
        a.report(a.viewer.setPresentation(p));
        break;
    }
    case GLFW_KEY_O:
        a.action(Action::Open);
        a.suppressCharacter = !(mods & (GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER));
        break;
    case GLFW_KEY_T:
        a.action(Action::Tiff);
        a.suppressCharacter = !(mods & (GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER));
        break;
    case GLFW_KEY_F:
        a.action(Action::Fit);
        break;
    case GLFW_KEY_W:
        a.action(Action::World);
        break;
    case GLFW_KEY_L:
        a.action(Action::Catalogue);
        break;
    case GLFW_KEY_V:
        a.action(Action::Charts);
        break;
    case GLFW_KEY_N:
        a.action(Action::Theme);
        break;
    case GLFW_KEY_R:
        a.action(Action::Radar);
        break;
    case GLFW_KEY_C:
        a.action(Action::Origin);
        break;
    case GLFW_KEY_P:
        a.action(Action::Pause);
        break;
    case GLFW_KEY_LEFT_BRACKET:
        a.action(Action::RangeDown);
        break;
    case GLFW_KEY_RIGHT_BRACKET:
        a.action(Action::RangeUp);
        break;
    case GLFW_KEY_I:
        a.action(Action::ImageVisible);
        break;
    case GLFW_KEY_J:
        a.action(Action::Reset);
        break;
    case GLFW_KEY_K:
        a.action(Action::ImageFit);
        break;
    case GLFW_KEY_COMMA:
        a.action(Action::ImageDown);
        break;
    case GLFW_KEY_PERIOD:
        a.action(Action::ImageUp);
        break;
    case GLFW_KEY_F5:
        if (!a.chartSource.empty())
            a.loadCharts(a.chartSource);
        break;
    }
}
void cursor(GLFWwindow* window, double x, double y) {
    auto& a = state(window);
    const auto previous = a.mapPoint(a.mouseX, a.mouseY), current = a.mapPoint(x, y);
    const double dx = x - a.mouseX, dy = y - a.mouseY;
    a.mouseX = x;
    a.mouseY = y;
    if (!a.mouseDown || !a.dragging)
        return;
    if (std::hypot(x - a.startX, y - a.startY) > 4)
        a.moved = true;
    if (a.editingImage && a.activeImage) {
        const auto before = a.viewer.screenToGeo(previous), after = a.viewer.screenToGeo(current);
        a.report(
            a.viewer.moveRaster(*a.activeImage, after.lon - before.lon, after.lat - before.lat));
    } else
        a.report(a.viewer.panBy(dx, dy));
}
void mouse(GLFWwindow* window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;
    auto& a = state(window);
    if (action == GLFW_PRESS) {
        if (a.prompt != Prompt::None)
            return;
        if (interfaceClick(a))
            return;
        if (a.catalogue)
            return;
        if (a.placingRadar) {
            a.report(a.viewer.setRadarOrigin(a.viewer.screenToGeo(a.mapPoint(a.mouseX, a.mouseY))));
            a.placingRadar = false;
            return;
        }
        a.viewer.rememberView();
        a.mouseDown = a.dragging = true;
        a.moved = false;
        a.startX = a.mouseX;
        a.startY = a.mouseY;
        const auto image = a.image();
        a.editingImage = (mods & GLFW_MOD_SHIFT) && image && image->visible;
    } else if (action == GLFW_RELEASE) {
        if (a.mouseDown && !a.moved && !a.editingImage && a.onMap(a.mouseX, a.mouseY)) {
            const auto position = a.mapPoint(a.mouseX, a.mouseY);
            const auto contacts = a.viewer.pickContacts(position);
            if (!contacts.empty() && !(mods & GLFW_MOD_CONTROL)) {
                a.selectedContactId = contacts.front().report.id;
                a.report(a.viewer.selectContact(a.selectedContactId));
                a.viewer.select(std::nullopt);
            } else {
                a.selectedContactId.reset();
                a.viewer.selectContact(std::nullopt);
                const auto hits = a.viewer.pickAll(position);
                std::optional<enc::FeatureId> id;
                if (!hits.empty()) {
                    std::size_t index = 0;
                    if ((mods & GLFW_MOD_CONTROL) && a.viewer.selected()) {
                        const auto selected = a.viewer.selected()->id;
                        const auto current =
                            std::find_if(hits.begin(), hits.end(),
                                         [&](const auto& f) { return f.id == selected; });
                        if (current != hits.end())
                            index = (std::distance(hits.begin(), current) + 1) % hits.size();
                    }
                    id = hits[index].id;
                    a.status = std::to_string(index + 1) + " / " + std::to_string(hits.size()) +
                               " OBJETOS";
                }
                a.report(a.viewer.select(id));
            }
            a.attributeScroll = 0;
        }
        a.mouseDown = a.dragging = a.editingImage = false;
    }
}
void scroll(GLFWwindow* window, double, double y) {
    auto& a = state(window);
    if (a.prompt != Prompt::None)
        return;
    if (a.mouseX >= a.mapWidth()) {
        const int delta = static_cast<int>(std::round(y * 3));
        if (a.mouseY < a.layerBottom())
            a.layerScroll = std::max(0, a.layerScroll - delta);
        else if (a.mouseY < a.overlayTop())
            a.attributeScroll = std::max(0, a.attributeScroll - delta);
        return;
    }
    if (!a.onMap(a.mouseX, a.mouseY) || a.catalogue)
        return;
    const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                       glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (shift && a.activeImage)
        a.report(a.viewer.scaleRasterAt(*a.activeImage,
                                        a.viewer.screenToGeo(a.mapPoint(a.mouseX, a.mouseY)),
                                        std::pow(1.15, y)));
    else {
        a.viewer.rememberView();
        a.report(a.viewer.zoomAt(a.mapPoint(a.mouseX, a.mouseY), std::pow(1.2, y)));
    }
}
void drop(GLFWwindow* window, int count, const char** paths) {
    auto& a = state(window);
    for (int i = 0; i < count; ++i)
        if (!tiff(paths[i]))
            a.loadCharts(paths[i]);
    for (int i = 0; i < count; ++i)
        if (tiff(paths[i]))
            a.openImage(paths[i]);
}
} // namespace
void registerInput(App& app) {
    glfwSetWindowUserPointer(app.window, &app);
    glfwSetKeyCallback(app.window, key);
    glfwSetCharCallback(app.window, character);
    glfwSetCursorPosCallback(app.window, cursor);
    glfwSetMouseButtonCallback(app.window, mouse);
    glfwSetScrollCallback(app.window, scroll);
    glfwSetDropCallback(app.window, drop);
}
} // namespace demo
