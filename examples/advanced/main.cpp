// main.cpp — Integración de carga progresiva, presentación configurable y contactos múltiples.
// Ejemplo sin paneles: el anfitrión conserva GLFW, entrada y contexto; el SDK gestiona todos los datos.
#include "../common/contact_scenario.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
namespace {
void check(const enc::Status& s) {
    if (!s)
        std::cerr << s.message << '\n';
}
struct App {
    enc::Viewer viewer;
    enc::Catalog catalog;
    std::unique_ptr<enc::ChartStreamer> stream;
    enc_examples::ContactScenario contacts;
    std::optional<enc::ContactId> selected;
    std::optional<enc::OverlayId> image;
    bool dragging = false, moved = false;
    double mouseX = 0, mouseY = 0, stableSince = 0;
    enc::Camera lastView;
    int width = 1280, height = 800, viewWidth = 1280, viewHeight = 800;
    void frame() {
        stream->poll(viewer);
        const auto camera = viewer.camera();
        if (camera.center.lon != lastView.center.lon || camera.center.lat != lastView.center.lat ||
            camera.pixelsPerDegree != lastView.pixelsPerDegree ||
            camera.rotationDegrees != lastView.rotationDegrees ||
            camera.projection != lastView.projection || width != viewWidth ||
            height != viewHeight) {
            lastView = camera;
            viewWidth = width;
            viewHeight = height;
            stableSince = glfwGetTime();
        }
        if (stableSince > 0 && !dragging && glfwGetTime() - stableSince > .2) {
            stableSince = 0;
            check(stream->requestView(viewer.viewBounds(100), viewer.scaleInfo().denominator));
        }
        check(contacts.advance(viewer));
    }
};
App& state(GLFWwindow* w) {
    return *static_cast<App*>(glfwGetWindowUserPointer(w));
}
void keys(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS)
        return;
    auto& a = state(w);
    auto camera = a.viewer.camera();
    auto p = a.viewer.presentation();
    auto contacts = a.viewer.contactSettings();
    switch (key) {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(w, GLFW_TRUE);
        return;
    case GLFW_KEY_Q:
    case GLFW_KEY_E:
        a.viewer.rememberView();
        camera.rotationDegrees += key == GLFW_KEY_Q ? -15 : 15;
        check(a.viewer.setCamera(camera));
        return;
    case GLFW_KEY_M:
        a.viewer.rememberView();
        camera.projection = camera.projection == enc::Projection::Mercator
                                ? enc::Projection::LocalEquirectangular
                                : enc::Projection::Mercator;
        check(a.viewer.setCamera(camera));
        return;
    case GLFW_KEY_F:
        a.viewer.rememberView();
        if (!a.viewer.fitCharts() && !a.catalog.cells().empty())
            check(a.viewer.fitBounds(a.catalog.cells()[0].bounds));
        return;
    case GLFW_KEY_H:
        a.viewer.previousView();
        return;
    case GLFW_KEY_Y:
        a.viewer.nextView();
        return;
    case GLFW_KEY_N:
        a.viewer.setTheme(a.viewer.theme() == enc::Theme::Day ? enc::Theme::Night
                                                              : enc::Theme::Day);
        return;
    case GLFW_KEY_D:
        p.displayMode = p.displayMode == enc::DisplayMode::All    ? enc::DisplayMode::Base
                        : p.displayMode == enc::DisplayMode::Base ? enc::DisplayMode::Standard
                                                                  : enc::DisplayMode::All;
        check(a.viewer.setPresentation(p));
        return;
    case GLFW_KEY_F6:
        p.preferNationalNames = !p.preferNationalNames;
        check(a.viewer.setPresentation(p));
        return;
    case GLFW_KEY_F8:
    case GLFW_KEY_F9:
        p.textScale = std::clamp(p.textScale + (key == GLFW_KEY_F9 ? .1 : -.1), .5, 3.);
        check(a.viewer.setPresentation(p));
        return;
    case GLFW_KEY_B:
        contacts.visible = !contacts.visible;
        check(a.viewer.configureContacts(contacts));
        return;
    case GLFW_KEY_S:
        contacts.showSleeping = !contacts.showSleeping;
        check(a.viewer.configureContacts(contacts));
        return;
    case GLFW_KEY_U:
        contacts.vectors = contacts.vectors == enc::VectorMode::True ? enc::VectorMode::Relative
                                                                     : enc::VectorMode::True;
        check(a.viewer.configureContacts(contacts));
        return;
    case GLFW_KEY_A:
        contacts.associatedAsAIS = !contacts.associatedAsAIS;
        check(a.viewer.configureContacts(contacts));
        return;
    case GLFW_KEY_G:
        if (a.selected)
            check(a.viewer.acknowledgeContact(*a.selected));
        return;
    case GLFW_KEY_I:
        if (a.image)
            for (const auto& image : a.viewer.rasters())
                if (image.id == *a.image)
                    check(a.viewer.setRasterVisible(image.id, !image.visible));
        return;
    case GLFW_KEY_R:
        a.viewer.enableRadar(!a.viewer.radarInfo().settings.visible);
        return;
    }
}
void mouse(GLFWwindow* w, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;
    auto& a = state(w);
    if (action == GLFW_PRESS) {
        a.dragging = true;
        a.moved = false;
        glfwGetCursorPos(w, &a.mouseX, &a.mouseY);
        a.viewer.rememberView();
    } else {
        a.dragging = false;
        if (a.moved)
            return;
        const enc::ScreenPoint p{a.mouseX, a.mouseY};
        const auto contacts = a.viewer.pickContacts(p);
        if (!contacts.empty() && !(mods & GLFW_MOD_CONTROL)) {
            a.selected = contacts.front().report.id;
            check(a.viewer.selectContact(a.selected));
            a.viewer.select(std::nullopt);
        } else {
            const auto hits = a.viewer.pickAll(p);
            std::optional<enc::FeatureId> selected;
            if (!hits.empty()) {
                std::size_t index = 0;
                const auto current = a.viewer.selected();
                if ((mods & GLFW_MOD_CONTROL) && current) {
                    const auto it = std::find_if(hits.begin(), hits.end(), [&](const auto& h) {
                        return h.id == current->id;
                    });
                    if (it != hits.end())
                        index = (std::distance(hits.begin(), it) + 1) % hits.size();
                }
                selected = hits[index].id;
                std::cout << hits[index].description << " / " << hits.size() << " objetos\n";
            }
            a.selected.reset();
            a.viewer.selectContact(std::nullopt);
            check(a.viewer.select(selected));
        }
    }
}
void cursor(GLFWwindow* w, double x, double y) {
    auto& a = state(w);
    if (a.dragging) {
        a.moved |= std::abs(x - a.mouseX) + std::abs(y - a.mouseY) > 1;
        check(a.viewer.panBy(x - a.mouseX, y - a.mouseY));
    }
    a.mouseX = x;
    a.mouseY = y;
}
void scroll(GLFWwindow* w, double, double delta) {
    auto& a = state(w);
    a.viewer.rememberView();
    check(a.viewer.zoomAt({a.mouseX, a.mouseY}, std::pow(1.2, delta)));
}
void screenshot(int width, int height, const std::filesystem::path& path) {
    std::vector<unsigned char> data(std::size_t(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y)
        out.write(reinterpret_cast<const char*>(data.data() + std::size_t(y) * width * 3),
                  width * 3);
}
} // namespace
int main(int argc, char** argv) {
    std::filesystem::path source = "sample-charts", capture;
    int smoke = 0;
    bool night = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--night")
                night = true;
            else if (arg == "--smoke-frames" && i + 1 < argc)
                smoke = std::stoi(argv[++i]);
            else if (arg == "--screenshot" && i + 1 < argc)
                capture = argv[++i];
            else if (arg == "--help") {
                std::cout << "Uso: enc-showcase [CARPETA_O_CELDA] [--night] [--smoke-frames N] "
                             "[--screenshot ARCHIVO.ppm]\nMouse: arrastrar/zoom/clic. Q/E rotar, M "
                             "proyeccion, D categorias, N noche, H/Y historial, B contactos, S "
                             "dormidos, U vectores, A asociados, G reconocer, I TIFF, R PPI, F "
                             "encuadrar, F6 idioma, F8/F9 texto.\n";
                return 0;
            } else
                source = arg;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    if (!glfwInit())
        return 2;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    auto* window = glfwCreateWindow(1280, 800, "ENC Showcase - Carga progresiva y contactos",
                                    nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 3;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    int result = 0;
    {
        App a;
        const auto indexed = a.catalog.scan(source);
        check(indexed);
        if (!indexed || a.catalog.cells().empty()) {
            result = 4;
        } else {
            a.viewer.resize(a.width, a.height);
            a.viewer.fitBounds(a.catalog.cells()[0].bounds);
            auto camera = a.viewer.camera();
            camera.projection = enc::Projection::Mercator;
            camera.rotationDegrees = 10;
            check(a.viewer.setCamera(camera));
            auto p = a.viewer.presentation();
            p.displayMode = enc::DisplayMode::Standard;
            p.preferNationalNames = true;
            check(a.viewer.setPresentation(p));
            enc::PortrayalRule coast;
            coast.objectClass = "COALNE";
            coast.category = enc::DisplayCategory::Base;
            coast.line = 0x4D4535;
            coast.lineWidth = 2;
            check(a.viewer.setPortrayalRules({coast}));
            a.viewer.setTheme(night ? enc::Theme::Night : enc::Theme::Day);
            const auto image = a.viewer.addDemoGeoTiff();
            if (image)
                a.image = image.value;
            check(a.contacts.initialize(a.viewer, a.width, a.height));
            a.stream = std::make_unique<enc::ChartStreamer>(a.catalog);
            check(
                a.stream->requestView(a.viewer.viewBounds(100), a.viewer.scaleInfo().denominator));
            a.lastView = a.viewer.camera();
            glfwSetWindowUserPointer(window, &a);
            glfwSetKeyCallback(window, keys);
            glfwSetMouseButtonCallback(window, mouse);
            glfwSetCursorPosCallback(window, cursor);
            glfwSetScrollCallback(window, scroll);
            double previous = glfwGetTime();
            const double started = previous;
            int frames = 0;
            bool initializedRadar = false;
            while (!glfwWindowShouldClose(window)) {
                const double now = glfwGetTime();
                check(a.viewer.advance(now - previous));
                previous = now;
                int fw = 0, fh = 0;
                glfwGetWindowSize(window, &a.width, &a.height);
                glfwGetFramebufferSize(window, &fw, &fh);
                if (a.width <= 0 || a.height <= 0 || fw <= 0 || fh <= 0) {
                    glfwWaitEventsTimeout(.05);
                    continue;
                }
                a.viewer.resize(a.width, a.height);
                a.frame();
                if (!a.stream->info().loading && !a.stream->info().lastResult) {
                    check(a.stream->info().lastResult);
                    result = 6;
                    break;
                }
                if (smoke > 0 && now - started > 30) {
                    std::cerr << "Tiempo agotado esperando cartas.\n";
                    result = 7;
                    break;
                }
                if (!initializedRadar && a.viewer.sceneInfo().cellCount) {
                    a.viewer.enableRadar(true);
                    initializedRadar = true;
                }
                const auto rendered = a.viewer.render({0, 0, fw, fh});
                if (!rendered) {
                    check(rendered);
                    result = 5;
                    break;
                }
                if (smoke > 0 && initializedRadar && ++frames >= smoke) {
                    if (!capture.empty())
                        screenshot(fw, fh, capture);
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                if (a.stream->info().loading)
                    glfwSetWindowTitle(window, "ENC Showcase - Cargando celdas en segundo plano");
                else
                    glfwSetWindowTitle(window, "ENC Showcase - Q/E rotar | M proyeccion | D "
                                               "categorias | G reconocer contactos");
                glfwSwapBuffers(window);
                glfwPollEvents();
            }
            check(a.viewer.releaseGraphicsResources());
        }
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
