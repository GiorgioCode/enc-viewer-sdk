// main.cpp — Visor de escritorio que consume exclusivamente la biblioteca enc::viewer.
// Posee GLFW, el contexto y el reloj; entrega a la biblioteca sólo datos, comandos y el área del mapa.
#include "app.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

namespace {
// Captura opcional para verificar visualmente el consumidor completo, incluidos sus paneles.
void saveScreenshot(int width, int height, const std::filesystem::path& path) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int row = height - 1; row >= 0; --row)
        output.write(reinterpret_cast<const char*>(pixels.data() +
                                                   static_cast<std::size_t>(row) * width * 3),
                     static_cast<std::streamsize>(width) * 3);
}
} // namespace
int main(int argc, char** argv) {
    std::filesystem::path source;
#ifdef ENC_DEMO_CHARTS
    source = ENC_DEMO_CHARTS;
#else
    source = std::filesystem::current_path() / "charts";
#endif
    std::vector<std::filesystem::path> images;
    bool night = false, radar = false, demo = false, contacts = false, streaming = false;
    int smokeFrames = 0;
    std::filesystem::path screenshot;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--night")
            night = true;
        else if (arg == "--day")
            night = false;
        else if (arg == "--radar")
            radar = true;
        else if (arg == "--contacts")
            contacts = true;
        else if (arg == "--streaming")
            streaming = true;
        else if (arg == "--demo-tiff")
            demo = true;
        else if (arg == "--smoke-frames" && i + 1 < argc) {
            try {
                smokeFrames = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "Número de cuadros inválido.\n";
                return 1;
            }
        } else if (arg == "--screenshot" && i + 1 < argc)
            screenshot = argv[++i];
        else if (arg == "--help") {
            std::cout
                << "Uso: enc-viewer-demo [CARPETA_O_CELDA] [IMAGEN.tif...] [--radar] [--demo-tiff] "
                   "[--night|--day] [--contacts] [--streaming]\nControles: O abrir, F ajustar, W "
                   "mundo, V ENC, N dia/noche, L "
                   "catalogo, R radar, C origen, P pausa, [ ] alcance, T TIFF, I ocultar, J "
                   "restaurar, K ajustar imagen, Shift+arrastre/rueda editar, F5 reindexar.\n";
            return 0;
        } else {
            auto ext = std::filesystem::path(arg).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".tif" || ext == ".tiff")
                images.emplace_back(arg);
            else
                source = arg;
        }
    }
    glfwSetErrorCallback([](int, const char* text) { std::cerr << "GLFW: " << text << '\n'; });
    if (!glfwInit())
        return 2;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow* window =
        glfwCreateWindow(1280, 800, "ENC SDK - Visor de integracion", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 3;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetWindowSizeLimits(window, 1000, 760, GLFW_DONT_CARE, GLFW_DONT_CARE);
    int exitCode = 0;
    {
        demo::App app;
        app.window = window;
        app.progressive = streaming;
        app.simulateContacts = contacts;
        app.viewer.resize(app.mapWidth(), app.mapHeight());
        app.viewer.setTheme(night ? enc::Theme::Night : enc::Theme::Day);
        demo::registerInput(app);
        app.loadCharts(source);
        if (app.viewer.sceneInfo().cellCount == 0 && !streaming)
            app.viewer.fitWorld();
        for (const auto& path : images)
            app.openImage(path);
        if (demo)
            app.demoImage();
        if (radar)
            app.viewer.enableRadar(true);
        if (contacts)
            app.createDemoContacts();
        double previous = glfwGetTime();
        int frames = 0;
        const double smokeStarted = glfwGetTime();
        while (!glfwWindowShouldClose(window)) {
            const double now = glfwGetTime();
            app.report(app.viewer.advance(now - previous));
            previous = now;
            glfwGetWindowSize(window, &app.width, &app.height);
            glfwGetFramebufferSize(window, &app.frameWidth, &app.frameHeight);
            if (app.width <= 0 || app.height <= 0 || app.frameWidth <= 0 || app.frameHeight <= 0) {
                glfwWaitEventsTimeout(.05);
                continue;
            }
            app.viewer.resize(app.mapWidth(), app.mapHeight());
            app.updateStreaming();
            app.updateDemoContacts();
            const double sx = static_cast<double>(app.frameWidth) / app.width,
                         sy = static_cast<double>(app.frameHeight) / app.height;
            const enc::RenderTarget target{0, static_cast<int>(demo::footer * sy),
                                           static_cast<int>(app.mapWidth() * sx),
                                           static_cast<int>(app.mapHeight() * sy)};
            const auto drawn = app.catalogue ? app.viewer.renderSymbolCatalogue(target)
                                             : app.viewer.render(target);
            if (!drawn) {
                app.report(drawn);
                exitCode = 4;
                break;
            }
            demo::drawInterface(app);
            if (!app.streamer ||
                (!app.streamer->info().loading && app.viewer.sceneInfo().cellCount > 0))
                ++frames;
            if (smokeFrames > 0 && frames >= smokeFrames) {
                if (!screenshot.empty())
                    saveScreenshot(app.frameWidth, app.frameHeight, screenshot);
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            if (smokeFrames > 0 && glfwGetTime() - smokeStarted > 30) {
                std::cerr << "Tiempo agotado esperando cartas de prueba.\n";
                exitCode = 4;
                break;
            }
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        // La liberación ocurre antes de que GLFW destruya el contexto; las imágenes CPU pueden sobrevivir.
        const auto released = app.viewer.releaseGraphicsResources();
        app.report(released);
        if (!released)
            exitCode = 5;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
