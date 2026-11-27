// chart_renderer.cpp — Renderizado del mundo y las cartas ENC.
// Compone cobertura, superficies, líneas, símbolos, selección y etiquetas sin colisiones.
// Contiene la implementación de esta responsabilidad.

#include "chart_renderer.h"
#include "camera.h"
#include "chart_style.h"
#include "render_primitives.h"
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

namespace enc::detail {

namespace {

// Las cajas y etiquetas existen únicamente durante el pintado de un cuadro.
struct LabelBox {
    double x, y, w, h;
};

struct ChartLabel {
    LabelBox box;
    std::string text;
    unsigned color;
};

class LabelLayout {
    std::vector<ChartLabel> labels;
    std::vector<LabelBox> occupied;

  public:
    // Reserva el cuerpo del símbolo antes de intentar ubicar etiquetas alrededor.
    void reserve(LabelBox box) {
        occupied.push_back(box);
    }
    // Rechaza etiquetas fuera del mapa o superpuestas a regiones ya ocupadas.
    bool add(const SceneState& app, double x, double y, std::string text, unsigned color,
             int maxWidth = 180) {
        if (text.empty())
            return false;
        const int width = std::min(static_cast<int>(text.size()) * 6, maxWidth);
        if (width <= 0 || x < 4 || x + width > app.width - kSidebarWidth - 4 ||
            y < kToolbarHeight + 4 || y + 9 > app.height - kStatusHeight - 4)
            return false;
        const LabelBox box{x - 2, y - 2, static_cast<double>(width + 4), 11};
        for (const auto& other : occupied)
            if (box.x < other.x + other.w && box.x + box.w > other.x && box.y < other.y + other.h &&
                box.y + box.h > other.y)
                return false;
        occupied.push_back(box);
        labels.push_back({{x, y, static_cast<double>(width), 7}, std::move(text), color});
        return true;
    }
    // Dibuja al final para que los símbolos posteriores no tapen los nombres aceptados.
    void draw() const {
        for (const auto& label : labels) {
            // Un fondo discreto mantiene legible el texto sobre líneas y profundidades.
            rect(label.box.x - 1, label.box.y - 1, label.box.w + 1, 9, 0xF6F5E9);
            drawText(label.box.x, label.box.y, label.text, label.color,
                     static_cast<int>(label.box.w));
        }
    }
};

// Pinta los triángulos ya calculados de una superficie geográfica.
void paintArea(const SceneState& app, const Feature& feature, unsigned color) {
    setHex(color);
    glBegin(GL_TRIANGLES);
    for (const auto& polygon : feature.polygons)
        for (Point p : polygon.triangles) {
            p = toScreen(app, p);
            glVertex2d(p.x, p.y);
        }
    glEnd();
}

// Dibuja siempre el fondo mundial y agrega una retícula sólo en escalas generales.
void drawBasemap(const SceneState& app) {
    // La retícula espaciada permite orientarse en las escalas mundial y continental.
    if (app.zoom < 300) {
        const int step = app.zoom < 12 ? 30 : app.zoom < 40 ? 10 : 1;
        setHex(0xACCBD4);
        glLineWidth(1);
        glBegin(GL_LINES);
        for (int lon = -180; lon <= 180; lon += step) {
            const Point a = toScreen(app, {static_cast<double>(lon), -90}),
                        b = toScreen(app, {static_cast<double>(lon), 90});
            glVertex2d(a.x, a.y);
            glVertex2d(b.x, b.y);
        }
        for (int lat = -90; lat <= 90; lat += step) {
            const Point a = toScreen(app, {-180, static_cast<double>(lat)}),
                        b = toScreen(app, {180, static_cast<double>(lat)});
            glVertex2d(a.x, a.y);
            glVertex2d(b.x, b.y);
        }
        glEnd();
    }
    for (const auto& feature : app.baseLand) {
        if (!inView(app, feature))
            continue;
        paintArea(app, feature, 0xDADCC8);
        setHex(0x8E9F95);
        glLineWidth(1);
        for (const auto& polygon : feature.polygons)
            for (const auto& ring : polygon.rings) {
                glBegin(GL_LINE_LOOP);
                for (Point p : ring) {
                    p = toScreen(app, p);
                    glVertex2d(p.x, p.y);
                }
                glEnd();
            }
    }
    if (app.zoom < 8) {
        struct Place {
            const char* name;
            double x, y;
        };
        const Place places[] = {{"NORTH AMERICA", -112, 43},
                                {"SOUTH AMERICA", -65, -15},
                                {"EUROPE", 16, 52},
                                {"AFRICA", 14, 7},
                                {"ASIA", 83, 43},
                                {"AUSTRALIA", 127, -25},
                                {"ANTARCTICA", 0, -80}};
        for (const auto& place : places) {
            Point p = toScreen(app, {place.x, place.y});
            drawText(p.x - std::char_traits<char>::length(place.name) * 3, p.y, place.name,
                     0x71857F);
        }
    }
}

// En detalle borra la costa generalizada dentro de la ENC; de lejos señala su cobertura.
void drawCoverage(const SceneState& app, bool overview) {
    for (const auto& f : app.features) {
        if (app.layers[f.layer].objectClass != "M_COVR" || attr(f, "CATCOV") != "1" ||
            !inView(app, f))
            continue;
        if (!overview) {
            // Sustituye la costa generalizada únicamente dentro de la cobertura ENC real.
            // Los huecos teselados y las áreas CATCOV=2 conservan el fondo mundial.
            paintArea(app, f, 0xC8E1DF);
        } else {
            setHex(0x8A5B94);
            glLineWidth(1.5f);
            for (const auto& polygon : f.polygons)
                for (const auto& ring : polygon.rings) {
                    glBegin(GL_LINE_LOOP);
                    for (Point p : ring) {
                        p = toScreen(app, p);
                        glVertex2d(p.x, p.y);
                    }
                    glEnd();
                }
            Point a = toScreen(app, {f.minX, f.minY}), b = toScreen(app, {f.maxX, f.maxY});
            if (std::abs(b.x - a.x) < 8 && std::abs(b.y - a.y) < 8) {
                const double x = (a.x + b.x) * 0.5, y = (a.y + b.y) * 0.5;
                rect(x - 3, y - 3, 6, 6, 0x8A5B94);
            }
        }
    }
}

// Prioriza ayudas, símbolos ordinarios y sondas, evitando colisiones entre etiquetas.
void drawPoints(SceneState& app) {
    LabelLayout layout;
    // Reserva espacio para las ayudas antes de ubicar puntos ordinarios y sondas.
    for (int pass = 0; pass < 3; ++pass)
        for (std::size_t fi = 0; fi < app.features.size(); ++fi) {
            const auto& f = app.features[fi];
            const bool selected = static_cast<int>(fi) == app.selected;
            const auto kind = f.symbol.kind;
            const int priority = isNavaid(kind) ? 0 : kind == nautical::Kind::Sounding ? 2 : 1;
            if (priority != pass || f.points.empty() || !pointVisible(app, f, selected))
                continue;
            const auto symbol = visibleSymbol(app, f);
            const double symbolScale =
                std::clamp(app.zoom / 6500.0, 0.72, 1.15) * app.presentation.symbolScale;
            for (const Point& world : f.points) {
                const Point p = toScreen(app, world);
                if (p.x < -30 || p.x > app.width - kSidebarWidth + 30 ||
                    p.y < kToolbarHeight - 35 || p.y > app.height - kStatusHeight + 35)
                    continue;
                if (kind == nautical::Kind::Sounding) {
                    if (!app.presentation.showSoundings || !std::isfinite(world.depth) ||
                        (app.zoom < 14000 && !selected))
                        continue;
                    std::ostringstream text;
                    text << std::fixed << std::setprecision(world.depth < 20 ? 1 : 0)
                         << world.depth;
                    layout.add(app, p.x + 2, p.y - 3, text.str(), 0x405A68, 48);
                    continue;
                }
                // SECTR1/2 están medidos desde el mar hacia la luz (IHO S-57, apéndice A).
                // Invierte 180° para dibujar desde la luz hacia el sector visible; admite cruce de 0°.
                if (app.presentation.showLightSectors &&
                    (kind == nautical::Kind::Light || symbol.lighted) &&
                    std::isfinite(symbol.sectorStart) && std::isfinite(symbol.sectorEnd)) {
                    double span = std::fmod(symbol.sectorEnd - symbol.sectorStart + 360.0, 360.0);
                    if (span == 0)
                        span = 360;
                    setHex(symbol.lightColour);
                    glLineWidth(1);
                    glBegin(GL_LINE_STRIP);
                    glVertex2d(p.x, p.y);
                    for (int step = 0; step <= 64; ++step) {
                        const double angle =
                            (symbol.sectorStart + 180 + span * step / 64) * kPi / 180;
                        glVertex2d(p.x + 65 * symbolScale * std::sin(angle),
                                   p.y - 65 * symbolScale * std::cos(angle));
                    }
                    glVertex2d(p.x, p.y);
                    glEnd();
                }
                nautical::draw(symbol, p.x, p.y, symbolScale);
                if (selected) {
                    glLineWidth(2);
                    nautical::circle(p.x, p.y, 12, 0xD16A30, false);
                }
                if (isNavaid(kind))
                    layout.reserve({p.x - 8, p.y - 27 * symbolScale, 17, 30 * symbolScale});
                if (app.presentation.showLabels &&
                    (selected || (app.zoom > 8500 && isNavaid(kind)) || app.zoom > 22000)) {
                    std::string name = attr(f, "OBJNAM");
                    if (name.empty() && selected)
                        name = app.layers[f.layer].objectClass;
                    layout.add(app, p.x + 12, p.y - 13, name, 0x263D43);
                }
            }
        }
    layout.draw();
}

} // namespace

// Pinta mundo, cobertura, superficies, líneas y símbolos en el orden de superposición.
void drawChart(SceneState& app) {
    const int mapW = std::max(1, app.width - kSidebarWidth),
              mapH = std::max(1, app.height - kToolbarHeight - kStatusHeight);
    const auto [sx, sy] = framebufferScale(app);
    glEnable(GL_SCISSOR_TEST);
    glScissor(app.viewportX, app.viewportY + static_cast<int>(kStatusHeight * sy),
              static_cast<int>(mapW * sx), static_cast<int>(mapH * sy));
    rect(0, kToolbarHeight, mapW, mapH, 0xB7D6DE);
    drawBasemap(app);
    if (!app.showCharts) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    // El resumen mundial evita saturar la pantalla con símbolos de celdas aún muy pequeñas.
    const bool overview = app.zoom < 800;
    drawCoverage(app, overview);
    if (overview) {
        if (!app.indexedCells.empty())
            drawText(16, kToolbarHeight + 16, "PURPLE: LOADED ENC COVERAGE / F: FIT CHARTS",
                     0x654B77, mapW - 30);
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    // Pinta las superficies físicas en el mismo orden para todas las celdas. Las áreas
    // administrativas (M_COVR, M_QUAL, SEAARE, etc.) no deben tapar tierra ni profundidades.
    std::array<std::vector<std::pair<const Feature*, unsigned int>>, 4> fillPasses;
    for (const Feature& feature : app.features) {
        if (!app.layers[feature.layer].visible || feature.polygons.empty() || !inView(app, feature))
            continue;
        const Style style = styleFor(app, feature);
        if (style.fillPass >= 0)
            fillPasses[static_cast<std::size_t>(style.fillPass)].push_back({&feature, style.fill});
    }
    for (const auto& pass : fillPasses)
        for (const auto& [feature, color] : pass) {
            paintArea(app, *feature, color);
        }
    for (std::size_t fi = 0; fi < app.features.size(); ++fi) {
        const Feature& feature = app.features[fi];
        if (!app.layers[feature.layer].visible || !inView(app, feature))
            continue;
        const Style style = styleFor(app, feature);
        const auto& objectClass = app.layers[feature.layer].objectClass;
        const bool specialArea = objectClass == "RESARE" || objectClass == "CTNARE" ||
                                 objectClass == "CBLARE" || objectClass == "PIPARE";
        const bool dashed = specialArea || objectClass == "CBLSUB" || objectClass == "PIPSOL";
        if (dashed) {
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(1, 0x0F0F);
        }
        setHex(specialArea ? nautical::magenta : style.line);
        glLineWidth(static_cast<int>(fi) == app.selected ? 2.5f : 1.0f);
        for (const auto& line : feature.lines) {
            glBegin(GL_LINE_STRIP);
            for (Point p : line) {
                p = toScreen(app, p);
                glVertex2d(p.x, p.y);
            }
            glEnd();
        }
        for (const Polygon& polygon : feature.polygons)
            for (const auto& ring : polygon.rings) {
                glBegin(GL_LINE_LOOP);
                for (Point p : ring) {
                    p = toScreen(app, p);
                    glVertex2d(p.x, p.y);
                }
                glEnd();
            }
        if (dashed)
            glDisable(GL_LINE_STIPPLE);
    }
    drawPoints(app);
    if (app.selected >= 0 && app.selected < static_cast<int>(app.features.size())) {
        const Feature& f = app.features[static_cast<std::size_t>(app.selected)];
        setHex(0xB94735);
        glLineWidth(2);
        for (const auto& line : f.lines) {
            glBegin(GL_LINE_STRIP);
            for (Point p : line) {
                p = toScreen(app, p);
                glVertex2d(p.x, p.y);
            }
            glEnd();
        }
        for (const Polygon& polygon : f.polygons)
            for (const auto& ring : polygon.rings) {
                glBegin(GL_LINE_LOOP);
                for (Point p : ring) {
                    p = toScreen(app, p);
                    glVertex2d(p.x, p.y);
                }
                glEnd();
            }
    }
    glDisable(GL_SCISSOR_TEST);
}

} // namespace enc::detail
