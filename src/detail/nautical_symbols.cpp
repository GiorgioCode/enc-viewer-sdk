// nautical_symbols.cpp — Catálogo vectorial y decodificación de simbología náutica.
// Interpreta códigos S-57 y dibuja cuerpos, marcas de tope, luces y otras familias de objetos.
// Contiene la implementación de esta responsabilidad.

#include "nautical_symbols.h"
#include "display_theme.h"
#include <GL/gl.h>

namespace enc::detail::nautical {

// Relaciona cada clase de objeto S-57 con una familia visual.
Kind kindFor(const std::string& name) {
    if (name.compare(0, 3, "BOY") == 0 || name == "LITFLT" || name == "LITVES")
        return Kind::Buoy;
    if (name.compare(0, 3, "BCN") == 0 || name == "DAYMAR")
        return Kind::Beacon;
    if (name == "TOPMAR")
        return Kind::Topmark;
    if (name == "LIGHTS")
        return Kind::Light;
    if (name == "SOUNDG")
        return Kind::Sounding;
    if (name == "UWTROC")
        return Kind::Rock;
    if (name == "WRECKS")
        return Kind::Wreck;
    if (name == "OBSTRN" || name == "DMPGRD")
        return Kind::Obstruction;
    if (name == "ACHARE" || name == "ACHBRT")
        return Kind::Anchorage;
    if (name == "MORFAC" || name == "BERTHS")
        return Kind::Mooring;
    if (name == "PILBOP")
        return Kind::Pilot;
    if (name == "SMCFAC" || name == "HRBFAC")
        return Kind::Marina;
    if (name == "LNDMRK")
        return Kind::Landmark;
    if (name == "BUISGL" || name == "FORSTC")
        return Kind::Building;
    if (name == "SILTNK")
        return Kind::Tank;
    if (name == "CRANES")
        return Kind::Crane;
    if (name == "PYLONS" || name == "PILPNT")
        return Kind::Pylon;
    if (name == "BRIDGE" || name == "GATCON")
        return Kind::Bridge;
    if (name == "FOGSIG")
        return Kind::FogSignal;
    if (name == "RTPBCN" || name == "RADSTA" || name == "RDOSTA")
        return Kind::Radar;
    if (name == "OFSPLF")
        return Kind::Platform;
    if (name == "WNDGEN")
        return Kind::WindTurbine;
    return Kind::Unknown;
}

// Extrae códigos numéricos de atributos que contienen listas o separadores.
std::vector<int> codes(const std::string& text) {
    std::vector<int> result;
    const char* cursor = text.c_str();
    while (*cursor) {
        char* end = nullptr;
        const long value = std::strtol(cursor, &end, 10);
        if (end != cursor) {
            result.push_back(static_cast<int>(value));
            cursor = end;
        } else
            ++cursor;
    }
    return result;
}

// Traduce los códigos COLOUR de S-57 a colores base de presentación.
unsigned colour(int code) {
    constexpr std::array<unsigned, 14> palette = {ink,      0xFFFFFF, ink,      red,      green,
                                                  0x447AC2, yellow,   0x8B9390, 0x926544, 0xE6AD38,
                                                  0x7856A2, 0xE98235, magenta,  0xE89CAD};
    return code > 0 && code < static_cast<int>(palette.size())
               ? palette[static_cast<std::size_t>(code)]
               : ink;
}

namespace {
struct V {
    double x, y;
};

// Aplica contraste nocturno según se trate de relleno o contorno.
void color(unsigned value, bool stroke = false) {
    value = display::symbolColour(value, stroke);
    glColor3ub(static_cast<GLubyte>(value >> 16), static_cast<GLubyte>(value >> 8),
               static_cast<GLubyte>(value));
}

// Traza líneas en coordenadas locales del símbolo.
void path(std::initializer_list<V> vertices, unsigned value, GLenum mode = GL_LINE_STRIP) {
    color(value, true);
    glBegin(mode);
    for (const auto& p : vertices)
        glVertex2d(p.x, p.y);
    glEnd();
}

// Rellena un contorno convexo o dibuja únicamente su borde.
void polygon(const std::vector<V>& vertices, unsigned value, bool fill = true) {
    if (vertices.empty())
        return;
    color(value, !fill);
    glBegin(fill ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
    if (fill) {
        V centre{0, 0};
        for (V p : vertices) {
            centre.x += p.x / vertices.size();
            centre.y += p.y / vertices.size();
        }
        glVertex2d(centre.x, centre.y);
    }
    for (const auto& p : vertices)
        glVertex2d(p.x, p.y);
    if (fill)
        glVertex2d(vertices.front().x, vertices.front().y);
    glEnd();
}

// Aproxima curvas con vértices para mantener el catálogo independiente de texturas.
std::vector<V> ellipse(double x, double y, double rx, double ry) {
    std::vector<V> vertices;
    for (int i = 0; i < 32; ++i) {
        const double angle = i * 6.283185307179586 / 32;
        vertices.push_back({x + rx * std::cos(angle), y + ry * std::sin(angle)});
    }
    return vertices;
}
} // namespace

// Reutiliza la elipse para puntos de anclaje, círculos y contornos.
void circle(double x, double y, double r, unsigned fill, bool solid) {
    polygon(ellipse(x, y, r, r), fill, solid);
}

namespace {
// Recorta un polígono contra un semiplano para formar bandas de color.
std::vector<V> clip(const std::vector<V>& vertices, double ax, double ay, double limit,
                    bool above) {
    std::vector<V> output;
    if (vertices.empty())
        return output;
    V previous = vertices.back();
    double before = ax * previous.x + ay * previous.y - limit;
    for (V current : vertices) {
        const double after = ax * current.x + ay * current.y - limit;
        if ((before >= 0) != (after >= 0)) {
            const double t = before / (before - after);
            output.push_back({previous.x + t * (current.x - previous.x),
                              previous.y + t * (current.y - previous.y)});
        }
        if ((after >= 0) == above)
            output.push_back(current);
        previous = current;
        before = after;
    }
    return output;
}

// Representa COLPAT con franjas, cuadros o bandas concéntricas recortadas al cuerpo.
void stripedBody(const std::vector<V>& hull, const Symbol& symbol) {
    if (hull.empty())
        return;
    const auto& colours = symbol.colours;
    if (colours.size() > 1 && symbol.pattern == 4) {
        double left = 1e30, right = -1e30, top = 1e30, bottom = -1e30;
        for (V p : hull) {
            left = std::min(left, p.x);
            right = std::max(right, p.x);
            top = std::min(top, p.y);
            bottom = std::max(bottom, p.y);
        }
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col) {
                const double x1 = left + (right - left) * col / 4,
                             x2 = left + (right - left) * (col + 1) / 4;
                const double y1 = top + (bottom - top) * row / 4,
                             y2 = top + (bottom - top) * (row + 1) / 4;
                const auto tile =
                    clip(clip(clip(clip(hull, 1, 0, x1, true), 1, 0, x2, false), 0, 1, y1, true), 0,
                         1, y2, false);
                polygon(tile, colours[static_cast<std::size_t>(row + col) % colours.size()]);
            }
        polygon(hull, ink, false);
        return;
    }
    if (colours.size() > 1 && symbol.pattern == 6) {
        V centre{0, 0};
        for (V p : hull) {
            centre.x += p.x / hull.size();
            centre.y += p.y / hull.size();
        }
        for (std::size_t i = 0; i < colours.size(); ++i) {
            const double factor = 1.0 - 0.6 * i / (colours.size() - 1);
            std::vector<V> ring;
            for (V p : hull)
                ring.push_back(
                    {centre.x + (p.x - centre.x) * factor, centre.y + (p.y - centre.y) * factor});
            polygon(ring, colours[i]);
        }
        polygon(hull, ink, false);
        return;
    }
    const bool vertical = symbol.pattern == 2;
    const bool diagonal = symbol.pattern == 3;
    const double ax = vertical || diagonal ? 1 : 0, ay = vertical ? 0 : 1;
    double low = 1e30, high = -1e30;
    for (V p : hull) {
        const double v = ax * p.x + ay * p.y;
        low = std::min(low, v);
        high = std::max(high, v);
    }
    const std::size_t count = std::max<std::size_t>(1, colours.size());
    for (std::size_t i = 0; i < count; ++i) {
        const double a = low + (high - low) * i / count, b = low + (high - low) * (i + 1) / count;
        const auto band = clip(clip(hull, ax, ay, a, true), ax, ay, b, false);
        polygon(band, colours.empty() ? ink : colours[i]);
    }
    polygon(hull, ink, false);
}

// Dibuja un cono de marca de tope orientado hacia arriba o hacia abajo.
void cone(double y, bool up, unsigned value) {
    polygon({{-3.2, y + (up ? 3 : -3)}, {3.2, y + (up ? 3 : -3)}, {0, y + (up ? -3 : 3)}}, value);
}

// Compone las marcas de tope según TOPSHP, incluidos los pares cardinales.
void topmark(int shape, unsigned value, double y = -22) {
    path({{0, y + 5}, {0, y + 9}}, ink);
    switch (shape) {
    case 1:
    case 24:
        cone(y, true, value);
        break;
    case 2:
    case 25:
        cone(y, false, value);
        break;
    case 3:
    case 26:
        circle(0, y, 3, value);
        break;
    case 4:
        circle(0, y - 3, 2.5, value);
        circle(0, y + 3, 2.5, value);
        break;
    case 10:
        cone(y - 3, false, value);
        cone(y + 3, true, value);
        break;
    case 11:
        cone(y - 3, true, value);
        cone(y + 3, false, value);
        break;
    case 13:
        cone(y - 3, true, value);
        cone(y + 3, true, value);
        break;
    case 14:
        cone(y - 3, false, value);
        cone(y + 3, false, value);
        break;
    case 7:
        path({{-3, y - 3}, {3, y + 3}}, value);
        path({{-3, y + 3}, {3, y - 3}}, value);
        break;
    case 8:
        path({{-4, y}, {4, y}}, value);
        path({{0, y - 4}, {0, y + 4}}, value);
        break;
    case 9:
    case 12:
        polygon({{0, y - 4}, {4, y}, {0, y + 4}, {-4, y}}, value);
        break;
    case 17:
        path({{0, y + 5}, {0, y - 5}}, ink);
        polygon({{0, y - 5}, {6, y - 3}, {0, y}}, value);
        break;
    case 5:
    case 6:
    case 19:
    case 20:
    case 21:
        polygon({{-3, y - 3}, {3, y - 3}, {3, y + 3}, {-3, y + 3}}, value);
        break;
    default: // Las formas no soportadas se indican mediante un rombo sin relleno.
        polygon({{0, y - 4}, {4, y}, {0, y + 4}, {-4, y}}, value, false);
        break;
    }
}

// Representa la presencia de una luz mediante un destello separado del cuerpo.
void flare(unsigned value) {
    // Desplaza el destello respecto del anclaje para mantener legible el cuerpo de la ayuda.
    path({{2, 2}, {16, 14}}, magenta);
    const double x = 17, y = 15;
    polygon({{x, y - 5},
             {x + 1.5, y - 1.5},
             {x + 5, y},
             {x + 1.5, y + 1.5},
             {x, y + 5},
             {x - 1.5, y + 1.5},
             {x - 5, y},
             {x - 1.5, y - 1.5}},
            value);
    circle(x, y, 1.5, ink);
}

// Construye directamente el contorno BOYSHP sin reasignar un vector inicialmente vacío.
std::vector<V> buoyHull(int shape) {
    switch (shape) {
    case 1:
        return {{-6, -2}, {0, -17}, {6, -2}};
    case 2:
        return {{-6, -14}, {6, -14}, {6, -2}, {-6, -2}};
    case 3:
        return ellipse(0, -8, 6, 6);
    case 5:
    case 8:
        return {{-2, -2}, {0, -20}, {3, -20}, {2, -2}};
    case 6:
        return ellipse(0, -7, 8, 5);
    case 7:
        return {{-9, -2}, {-7, -14}, {7, -14}, {9, -2}};
    case 4:
        return {{-6, -2}, {-3, -7}, {-2, -17}, {2, -17}, {3, -7}, {6, -2}};
    default:
        return {{-6, -2}, {-4, -9}, {4, -9}, {6, -2}};
    }
}

// Construye el ancla usada por fondeaderos y puertos deportivos.
void anchor(unsigned value) {
    circle(0, -12, 2, value, false);
    path({{0, -10}, {0, 4}}, value);
    path({{-5, -6}, {5, -6}}, value);
    path({{-8, -1}, {-6, 3}, {0, 6}, {6, 3}, {8, -1}}, value);
    path({{-8, -1}, {-8, 3}}, value);
    path({{8, -1}, {8, 3}}, value);
}
} // namespace

// Dibuja cada familia con origen geográfico común y tamaño lógico estable en pantalla.
void draw(const Symbol& symbol, double x, double y, double scale) {
    glPushMatrix();
    glTranslated(x, y, 0);
    glScaled(scale, scale, 1);
    glLineWidth(1.4f);
    switch (symbol.kind) {
    case Kind::Buoy: {
        path({{-9, 1}, {-5, 2}, {0, 1}, {5, 2}, {9, 1}}, 0x527E91);
        const auto hull = buoyHull(symbol.shape);
        stripedBody(hull, symbol);
        circle(0, 0, 1.5, ink);
        if (symbol.topShape)
            topmark(symbol.topShape, symbol.topColour, symbol.shape == 5 ? -26 : -23);
        if (symbol.lighted)
            flare(symbol.lightColour);
        break;
    }
    case Kind::Beacon:
        path({{-7, 1}, {7, 1}}, ink);
        if (symbol.shape == 3 || symbol.shape == 4) {
            stripedBody({{-5, 0}, {-3, -18}, {3, -18}, {5, 0}}, symbol);
            if (symbol.shape == 4) {
                path({{-4, -2}, {3, -12}, {-3, -12}, {4, -2}}, ink);
            }
        } else {
            path({{0, 0}, {0, -17}}, ink);
            if (!symbol.topShape) {
                Symbol head = symbol;
                stripedBody({{-5, -18}, {5, -18}, {5, -10}, {-5, -10}}, head);
            }
        }
        if (symbol.topShape)
            topmark(symbol.topShape, symbol.topColour, -22);
        if (symbol.lighted)
            flare(symbol.lightColour);
        break;
    case Kind::Topmark:
        topmark(symbol.topShape, symbol.topColour);
        break;
    case Kind::Light:
        circle(0, 0, 3, 0xFFFFFF);
        circle(0, 0, 3, ink, false);
        flare(symbol.lightColour);
        break;
    case Kind::Rock:
        if (symbol.waterLevel == 1 || symbol.waterLevel == 2) {
            polygon({{-7, 3}, {-3, -6}, {1, -2}, {4, -8}, {8, 3}}, 0xE2D3A6);
            path({{-7, 3}, {-3, -6}, {1, -2}, {4, -8}, {8, 3}, {-7, 3}}, ink);
        } else {
            circle(0, 0, 7, ink, false);
            path({{-5, 0}, {5, 0}}, ink);
            path({{0, -5}, {0, 5}}, ink);
            if (symbol.waterLevel == 5) {
                for (int i = 0; i < 4; ++i)
                    circle((i % 2 ? 1 : -1) * 4, (i / 2 ? 1 : -1) * 4, 1, ink);
            } else
                circle(0, 0, 1.5, ink);
        }
        break;
    case Kind::Wreck:
        path({{-10, 1}, {10, 1}}, ink);
        path({{-8, -3}, {-8, 5}}, ink);
        path({{0, -5}, {0, 5}}, ink);
        path({{8, -3}, {8, 5}}, ink);
        path({{-10, -5}, {10, 7}}, ink);
        break;
    case Kind::Obstruction:
        polygon({{0, -7}, {7, 0}, {0, 7}, {-7, 0}}, ink, false);
        path({{-3, -3}, {3, 3}}, ink);
        path({{-3, 3}, {3, -3}}, ink);
        break;
    case Kind::Anchorage:
        anchor(magenta);
        break;
    case Kind::Mooring:
        circle(0, 0, 5, ink, false);
        path({{-7, 0}, {7, 0}}, ink);
        break;
    case Kind::Pilot:
        polygon({{0, -8}, {7, 5}, {-7, 5}}, magenta, false);
        circle(0, 0, 2, magenta);
        break;
    case Kind::Marina:
        anchor(magenta);
        circle(0, -3, 12, magenta, false);
        break;
    case Kind::Landmark:
        if (symbol.category == 18 || symbol.category == 19) {
            path({{0, 3}, {0, -12}, {0, -23}}, ink);
            path({{0, -12}, {-10, -6}}, ink);
            path({{0, -12}, {10, -6}}, ink);
        } else if (symbol.category == 3 || symbol.category == 6) {
            polygon({{-3, 0}, {-3, -19}, {3, -19}, {3, 0}}, 0xE9E3CE);
            path({{-3, 0}, {-3, -19}, {3, -19}, {3, 0}}, ink);
            path({{3, -19}, {7, -22}, {4, -25}, {8, -28}}, ink);
        } else if (symbol.category == 14) {
            path({{0, 0}, {0, -21}}, ink);
            path({{-6, -14}, {6, -14}}, ink);
        } else {
            polygon({{-5, 0}, {-3, -16}, {0, -22}, {3, -16}, {5, 0}}, 0xF5F1E5);
            path({{-5, 0}, {-3, -16}, {0, -22}, {3, -16}, {5, 0}, {-5, 0}}, ink);
            path({{-2, -10}, {2, -10}}, ink);
        }
        break;
    case Kind::Building:
        polygon({{-6, 3}, {-6, -7}, {0, -12}, {6, -7}, {6, 3}}, 0xBEB8A8);
        path({{-6, 3}, {-6, -7}, {0, -12}, {6, -7}, {6, 3}, {-6, 3}}, ink);
        path({{0, 3}, {0, -3}}, ink);
        break;
    case Kind::Tank:
        polygon(ellipse(0, -2, 6, 6), 0xD8D1BC);
        circle(0, -2, 6, ink, false);
        path({{-4, -6}, {4, 2}}, ink);
        path({{-4, 2}, {4, -6}}, ink);
        break;
    case Kind::Crane:
        path({{-5, 2}, {5, 2}, {0, 2}, {0, -18}, {12, -14}, {-7, -14}, {0, -18}}, ink);
        path({{12, -14}, {12, -3}, {9, -3}}, ink);
        break;
    case Kind::Pylon:
        path({{-6, 2}, {0, -19}, {6, 2}}, ink);
        path({{-6, -12}, {6, -12}}, ink);
        path({{-4, -4}, {4, -4}, {-3, -10}, {3, -10}}, ink);
        break;
    case Kind::Bridge:
        path({{-10, -5}, {10, -5}}, ink);
        path({{-10, 3}, {10, 3}}, ink);
        path({{-7, -9}, {-7, 7}}, ink);
        path({{7, -9}, {7, 7}}, ink);
        break;
    case Kind::FogSignal:
        for (int i = 0; i < 3; ++i) {
            const double r = 4 + i * 4;
            std::vector<V> arc;
            for (int n = 0; n <= 12; ++n) {
                const double a = -0.8 + n * 1.6 / 12;
                arc.push_back({r * std::cos(a), r * std::sin(a)});
            }
            color(magenta);
            glBegin(GL_LINE_STRIP);
            for (V p : arc)
                glVertex2d(p.x, p.y);
            glEnd();
        }
        circle(0, 0, 2, ink);
        break;
    case Kind::Radar:
        circle(0, 0, 7, magenta, false);
        path({{0, 0}, {6, -6}}, magenta);
        circle(0, 0, 1.5, ink);
        break;
    case Kind::WindTurbine:
        path({{0, 3}, {0, -12}, {0, -23}}, ink);
        path({{0, -12}, {-10, -6}}, ink);
        path({{0, -12}, {10, -6}}, ink);
        break;
    case Kind::Platform:
        polygon({{-8, -12}, {8, -12}, {8, -5}, {-8, -5}}, 0xC5BCAB);
        path({{-8, -12}, {8, -12}, {8, -5}, {-8, -5}, {-8, -12}}, ink);
        path({{-6, -5}, {-8, 3}}, ink);
        path({{6, -5}, {8, 3}}, ink);
        path({{2, -12}, {2, -22}, {6, -12}}, ink);
        break;
    case Kind::Sounding:
        circle(0, 0, 1, 0x526970);
        break;
    case Kind::Unknown:
        polygon({{0, -3}, {3, 0}, {0, 3}, {-3, 0}}, 0x64777B, false);
        break;
    }
    glPopMatrix();
}

} // namespace enc::detail::nautical
