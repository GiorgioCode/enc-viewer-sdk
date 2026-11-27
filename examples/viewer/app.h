// app.h — Estado y contratos de la aplicación consumidora de enc::viewer.
// La ventana, los diálogos, controles y callbacks están aquí, fuera de la biblioteca reutilizable.
#pragma once
#include "../common/contact_scenario.h"
#include <GLFW/glfw3.h>
#include <enc/enc.h>
#include <optional>
#include <vector>
namespace demo {
constexpr int sidebar = 350, toolbar = 56, footer = 24;
enum class Prompt { None, Charts, Tiff };
enum class Action {
    Open,
    Fit,
    World,
    Catalogue,
    Theme,
    Charts,
    Radar,
    Pause,
    Origin,
    RangeDown,
    RangeUp,
    GainDown,
    GainUp,
    RadarDown,
    RadarUp,
    Tiff,
    Demo,
    ImageVisible,
    ImageDown,
    ImageUp,
    ImageFit,
    Previous,
    Next,
    Reset,
    Remove,
    ToTop
};
struct Button {
    double x, y, width, height;
    std::string label;
    Action action;
    bool active = false;
};
struct App {
    GLFWwindow* window = nullptr;
    enc::Viewer viewer;
    enc::Catalog catalog;
    bool progressive = false, simulateContacts = false;
    std::unique_ptr<enc::ChartStreamer> streamer;
    std::uint64_t streamReportedRevision = 0;
    std::optional<enc::Camera> streamView;
    int streamWidth = 0, streamHeight = 0;
    double cameraStableSince = 0;
    enc_examples::ContactScenario contactScenario;
    std::optional<enc::ContactId> selectedContactId;
    int width = 1280, height = 800, frameWidth = 1280, frameHeight = 800;
    int layerScroll = 0, attributeScroll = 0;
    double mouseX = 0, mouseY = 0, startX = 0, startY = 0;
    bool mouseDown = false, dragging = false, moved = false, editingImage = false;
    bool catalogue = false, placingRadar = false, suppressCharacter = false;
    Prompt prompt = Prompt::None;
    std::string promptText, status = "ENC SDK / LISTO";
    std::filesystem::path chartSource;
    std::optional<enc::OverlayId> activeImage;
    int mapWidth() const {
        return width - sidebar;
    }
    int mapHeight() const {
        return height - toolbar - footer;
    }
    int layerBottom() const;
    int overlayTop() const {
        return height - 338;
    }
    bool onMap(double x, double y) const;
    enc::ScreenPoint mapPoint(double x, double y) const {
        return {x, y - toolbar};
    }
    void report(const enc::Status& status);
    void updateStreaming();
    void createDemoContacts();
    void updateDemoContacts();
    void loadCharts(const std::filesystem::path& source);
    void openImage(const std::filesystem::path& source);
    void demoImage();
    void openPrompt(Prompt type);
    std::optional<enc::RasterInfo> image() const;
    void action(Action action);
};
std::vector<Button> buttons(const App& app);
void drawInterface(App& app);
bool interfaceClick(App& app);
void registerInput(App& app);
} // namespace demo
