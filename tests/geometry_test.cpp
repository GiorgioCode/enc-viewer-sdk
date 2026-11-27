// geometry_test.cpp — Comprobación del algoritmo privado de teselación sin contexto OpenGL.
// Contrasta área total y ausencia de relleno dentro de huecos, sin depender del resultado de una captura.
#include "detail/geometry.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main() {
    try {
        enc::detail::Polygon polygon;
        polygon.rings = {{{-2, -2}, {2, -2}, {2, 2}, {-2, 2}, {-2, -2}},
                         {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}}};
        enc::detail::triangulate(polygon);
        double area = 0;
        for (std::size_t i = 0; i + 2 < polygon.triangles.size(); i += 3) {
            const auto a = polygon.triangles[i], b = polygon.triangles[i + 1],
                       c = polygon.triangles[i + 2];
            area += std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * .5;
            if (enc::detail::inRing({(a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3},
                                    polygon.rings[1]))
                throw std::runtime_error("Un triángulo rellena el hueco interior.");
        }
        if (std::abs(area - 12) > 1e-9)
            throw std::runtime_error("El área teselada no descuenta el hueco.");
        std::cout << "PASS: teselación sin contexto, anillos con igual orientación y huecos "
                     "conservados.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
