// opengl.cpp — Integración mínima del renderizador en una ventana GLFW sin paneles.
// Muestra las únicas responsabilidades gráficas del anfitrión: contexto, tamaño, tiempo y presentación.
#include <GLFW/glfw3.h>
#include <cmath>
#include <enc/enc.h>
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Uso: enc-minimal CARPETA_O_CELDA\n";
        return 1;
    }
    if (!glfwInit())
        return 2;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    auto* window = glfwCreateWindow(1024, 768, "Integracion minima ENC", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 3;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    {
        enc::Viewer viewer;
        viewer.resize(1024, 768);
        const auto loaded = viewer.loadFolder(argv[1]);
        if (!loaded)
            std::cerr << loaded.message << '\n';
        glfwSetWindowUserPointer(window, &viewer);
        glfwSetScrollCallback(window, [](GLFWwindow* window, double, double y) {
            double x = 0, cursorY = 0;
            glfwGetCursorPos(window, &x, &cursorY);
            static_cast<enc::Viewer*>(glfwGetWindowUserPointer(window))
                ->zoomAt({x, cursorY}, std::pow(1.2, y));
        });
        double previous = glfwGetTime();
        while (!glfwWindowShouldClose(window)) {
            int w = 0, h = 0, fw = 0, fh = 0;
            glfwGetWindowSize(window, &w, &h);
            glfwGetFramebufferSize(window, &fw, &fh);
            viewer.resize(w, h);
            const double now = glfwGetTime();
            viewer.advance(now - previous);
            previous = now;
            const auto rendered = viewer.render({0, 0, fw, fh});
            if (!rendered) {
                std::cerr << rendered.message << '\n';
                break;
            }
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        viewer.releaseGraphicsResources();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
