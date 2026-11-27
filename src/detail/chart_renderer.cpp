// chart_renderer.cpp — Renderizado del mundo y las cartas ENC.
// Compone cobertura, superficies, líneas, símbolos, selección y etiquetas sin colisiones.
// Contiene la implementación de esta responsabilidad.

#include "chart_renderer.h"
#include "camera.h"
#include "chart_style.h"
#include "mosaic.h"
#include "render_primitives.h"
#include "text_renderer.h"
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
    void reserve(LabelBox box) {
        occupied.push_back(box);
    }
    bool add(SceneState& app, double x, double y, std::string text, unsigned color,
             int maxWidth = 180, std::uint64_t key = 0, bool selected = false) {
        if (text.empty() ||
            (!selected &&
             labels.size() >= std::size_t(std::max(1., app.width * app.height / 9000. *
                                                           app.presentation.labelDensity))))
            return false;
        const double scale = app.presentation.textScale;
        const double width = textWidth(text, scale);
        if (width > maxWidth * scale)
            return false; // Nunca cortar una secuencia UTF-8 por bytes.
        const double height = 24 * scale;
        const std::array<ScreenPoint, 6> offsets{{{0, 0},
                                                  {-width - 24, 0},
                                                  {0, 18},
                                                  {-width - 24, 18},
                                                  {-width * .5 - 12, -24},
                                                  {-width * .5 - 12, 30}}};
        const auto old = app.labelPositions.find(key);
        const int preferred = old == app.labelPositions.end() ? 0 : old->second;
        for (int attempt = 0; attempt < 6; ++attempt) {
            const int index =
                attempt == 0 ? preferred : (attempt <= preferred ? attempt - 1 : attempt);
            const auto offset = offsets[index];
            const double padding = 2 / app.presentation.labelDensity;
            LabelBox box{x + offset.x - padding, y + offset.y - padding, width + 2 * padding,
                         height + 2 * padding};
            if (box.x < 4 || box.y < 4 || box.x + box.w > app.width - 4 ||
                box.y + box.h > app.height - 4)
                continue;
            bool collision = false;
            for (const auto& other : occupied)
                collision |= box.x < other.x + other.w && box.x + box.w > other.x &&
                             box.y < other.y + other.h && box.y + box.h > other.y;
            if (collision)
                continue;
            occupied.push_back(box);
            app.labelPositions[key] = index;
            labels.push_back({{x + offset.x, y + offset.y, width, height}, std::move(text), color});
            return true;
        }
        return false;
    }
    void draw(SceneState& app) const {
        for (const auto& label : labels) {
            rect(label.box.x - 1, label.box.y, label.box.w + 2, label.box.h, 0xF6F5E9);
            drawLabelText(app, label.box.x, label.box.y, label.text, label.color,
                          app.presentation.textScale);
        }
    }
};

// Proyecta cada contorno en una rama continua y repite sólo las copias mundiales que llegan a la vista.
std::vector<int> worldCopies(const SceneState& app, const std::vector<Point>& points) {
    std::vector<int> result;
    if (points.empty())
        return result;
    double x1 = points[0].x, x2 = x1, y1 = points[0].y, y2 = y1;
    for (auto p : points) {
        x1 = std::min(x1, p.x);
        x2 = std::max(x2, p.x);
        y1 = std::min(y1, p.y);
        y2 = std::max(y2, p.y);
    }
    for (int copy : {-1, 0, 1}) {
        double lo = 1e30, hi = -1e30, top = 1e30, bottom = -1e30;
        for (auto p : std::array<Point, 4>{{{x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}}}) {
            p = toScreenContinuous(app, p, points[0].x, copy);
            lo = std::min(lo, p.x);
            hi = std::max(hi, p.x);
            top = std::min(top, p.y);
            bottom = std::max(bottom, p.y);
        }
        if (hi >= -50 && lo <= app.width + 50 && bottom >= -50 && top <= app.height + 50)
            result.push_back(copy);
    }
    return result;
}
void drawGeoPath(const SceneState& app, const std::vector<Point>& points, GLenum primitive) {
    for (auto copy : worldCopies(app, points)) {
        glBegin(primitive);
        for (auto p : points) {
            p = toScreenContinuous(app, p, points.front().x, copy);
            glVertex2d(p.x, p.y);
        }
        glEnd();
    }
}
// Usa el mismo anclaje para todos los triángulos: evita franjas que crucen el mapa en ±180°.
void paintArea(const SceneState& app, const Feature& feature, unsigned color) {
    setHex(color);
    for (const auto& polygon : feature.polygons) {
        if (polygon.triangles.empty())
            continue;
        const auto& outline = polygon.rings.empty() ? polygon.triangles : polygon.rings.front();
        for (auto copy : worldCopies(app, outline)) {
            glBegin(GL_TRIANGLES);
            for (auto p : polygon.triangles) {
                p = toScreenContinuous(app, p, outline.front().x, copy);
                glVertex2d(p.x, p.y);
            }
            glEnd();
        }
    }
}

// Dibuja siempre el fondo mundial y agrega una retícula sólo en escalas generales.
void drawBasemap(const SceneState& app) {
    // La retícula espaciada permite orientarse en las escalas mundial y continental.
    if (app.zoom < 300) {
        const int step = app.zoom < 12 ? 30 : app.zoom < 40 ? 10 : 1;
        setHex(0xACCBD4);
        glLineWidth(1);
        for (int lon = -180; lon <= 180; lon += step)
            drawGeoPath(app, {{static_cast<double>(lon), -90}, {static_cast<double>(lon), 90}},
                        GL_LINE_STRIP);
        for (int lat = -90; lat <= 90; lat += step)
            drawGeoPath(app, {{-180, static_cast<double>(lat)}, {180, static_cast<double>(lat)}},
                        GL_LINE_STRIP);
    }

    for (const auto& feature : app.baseLand) {
        if (!inView(app, feature))
            continue;
        paintArea(app, feature, 0xDADCC8);
        setHex(0x8E9F95);
        glLineWidth(1);
        for (const auto& polygon : feature.polygons)
            for (const auto& ring : polygon.rings)
                drawGeoPath(app, ring, GL_LINE_LOOP);
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
    for (auto cell : activeCells(app)) {
        const auto& coverage = app.cells[cell].coverage;
        if (!overview) {
            if (!app.presentation.automaticMosaic) {
                Feature f;
                f.polygons = coverage;
                paintArea(app, f, 0xC8E1DF);
            }
            continue;
        }
        setHex(0x8A5B94);
        glLineWidth(1.5f);
        for (const auto& polygon : coverage)
            for (const auto& ring : polygon.rings) {
                drawGeoPath(app, ring, GL_LINE_LOOP);
                if (ring.empty())
                    continue;
                const auto a = toScreen(app, ring.front());
                const auto b = toScreenContinuous(app, ring[ring.size() / 2], ring.front().x);
                if (std::abs(a.x - b.x) < 8 && std::abs(a.y - b.y) < 8)
                    rect((a.x + b.x) * .5 - 3, (a.y + b.y) * .5 - 3, 6, 6, 0x8A5B94);
            }
    }
}

// Prioriza ayudas, símbolos ordinarios y sondas, evitando colisiones entre etiquetas.
void drawPoints(SceneState& app, const std::vector<std::size_t>& indices) {
    LabelLayout layout;
    // Reserva espacio para las ayudas antes de ubicar puntos ordinarios y sondas.
    for (int pass = 0; pass < 3; ++pass)
        for (auto fi : indices) {
            const auto& f = app.features[fi];
            const bool selected = static_cast<int>(fi) == app.selected;
            const auto kind = f.symbol.kind;
            const int priority = isNavaid(kind) ? 0 : kind == nautical::Kind::Sounding ? 2 : 1;
            if (priority != pass || f.points.empty() || !pointVisible(app, f, selected))
                continue;
            const auto symbol = visibleSymbol(app, f);
            const double symbolScale =
                std::clamp(app.zoom / 6500.0, 0.72, 1.15) * app.presentation.symbolScale;
            std::size_t pointNumber = 0;
            for (const Point& world : f.points) {
                const std::uint64_t labelKey =
                    (static_cast<std::uint64_t>(fi) << 32) | pointNumber++;
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
                    layout.add(app, p.x + 2, p.y - 3, text.str(),
                               world.depth <= app.presentation.safetyDepth ? 0xA13D32 : 0x405A68,
                               64, labelKey, selected);
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
                            (symbol.sectorStart + 180 + span * step / 64 + app.rotation) * kPi /
                            180;
                        glVertex2d(p.x + 65 * symbolScale * std::sin(angle),
                                   p.y - 65 * symbolScale * std::cos(angle));
                    }
                    glVertex2d(p.x, p.y);
                    glEnd();
                }
                nautical::draw(symbol, p.x, p.y, symbolScale);
                if (dangerous(app, f)) {
                    glLineWidth(2);
                    nautical::circle(p.x, p.y, 13 * symbolScale, 0xA13D32, false);
                }
                if (selected) {
                    glLineWidth(2);
                    nautical::circle(p.x, p.y, 12, 0xD16A30, false);
                }
                if (isNavaid(kind))
                    layout.reserve({p.x - 8, p.y - 27 * symbolScale, 17, 30 * symbolScale});
                if (app.presentation.showLabels &&
                    (selected || (app.zoom > 8500 && isNavaid(kind)) || app.zoom > 22000)) {
                    std::string name = app.presentation.preferNationalNames ? attr(f, "NOBJNM")
                                                                            : attr(f, "OBJNAM");
                    if (name.empty())
                        name = attr(f, "OBJNAM");
                    if (name.empty() && selected)
                        name = app.layers[f.layer].objectClass;
                    layout.add(app, p.x + 12, p.y - 13, name, 0x263D43, 240, labelKey, selected);
                }
            }
        }
    layout.draw(app);
}

void drawDecorations(SceneState& app) {
    if (app.presentation.showScaleBar) {
        const double mpp = metresPerPixel(app),
                     desired = std::max(1e-9, mpp * std::min(140., app.width * .25));
        const double decade = std::pow(10., std::floor(std::log10(desired))),
                     normal = desired / decade;
        const double distance = decade * (normal >= 5   ? 5
                                          : normal >= 2 ? 2
                                                        : 1),
                     pixels = distance / mpp;
        const double y = app.height - 24;
        rect(12, y, pixels, 3, 0x263D43);
        rect(12, y - 3, 1, 9, 0x263D43);
        rect(12 + pixels, y - 3, 1, 9, 0x263D43);
        std::ostringstream text;
        text << std::setprecision(3) << (distance >= 1000 ? distance / 1000 : distance)
             << (distance >= 1000 ? " km" : " m");
        drawLabelText(app, 14, y - 24, text.str(), 0x263D43, .8);
    }
    if (app.showCharts && app.presentation.showOverscale) {
        bool overscale = false;
        for (auto i : activeCells(app, 0))
            overscale |= app.cells[i].compilationScale > displayScale(app);
        if (overscale)
            drawLabelText(app, 14, 8, "OVERSCALE", 0xA13D32, .9);
    }
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
        drawDecorations(app);
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
        drawDecorations(app);
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const auto indices = app.spatial.query(viewportBounds(app, 50));
    struct ResetCell {
        int& value;
        ~ResetCell() {
            value = -1;
        }
    } reset{app.renderingCell};
    for (auto cellIndex : activeCells(app)) {
        app.renderingCell = static_cast<int>(cellIndex);
        const auto contour = safetyContour(app, app.renderingCell);
        // El pintado por celda elimina símbolos y líneas generales cubiertos por datos más detallados.
        if (app.presentation.automaticMosaic && app.cells[cellIndex].hasCoverage) {
            Feature coverage;
            coverage.polygons = app.cells[cellIndex].coverage;
            paintArea(app, coverage, 0xC8E1DF);
        }
        std::vector<std::size_t> visible;
        for (auto index : indices)
            if (featureVisible(app, app.features[index], static_cast<int>(index) == app.selected))
                visible.push_back(index);
        std::stable_sort(visible.begin(), visible.end(), [&](auto a, auto b) {
            const auto* ra = ruleFor(app, app.layers[app.features[a].layer].objectClass);
            const auto* rb = ruleFor(app, app.layers[app.features[b].layer].objectClass);
            return (ra ? ra->priority : 5) < (rb ? rb->priority : 5);
        });
        for (int pass = 0; pass < 4; ++pass)
            for (auto i : visible) {
                const auto& f = app.features[i];
                const auto style = styleFor(app, f);
                if (style.fillPass == pass)
                    paintArea(app, f, style.fill);
            }
        for (auto i : visible) {
            const auto& f = app.features[i];
            const auto style = styleFor(app, f);
            const auto& name = app.layers[f.layer].objectClass;
            const auto* rule = ruleFor(app, name);
            const bool isContour = name == "DEPCNT" && contour && !attr(f, "VALDCO").empty() &&
                                   std::abs(std::atof(attr(f, "VALDCO").c_str()) - *contour) < 1e-6;
            const bool selected = static_cast<int>(i) == app.selected;
            setHex(selected ? 0xB94735 : isContour ? 0x304B50 : style.line);
            glLineWidth(selected ? 2.5f : isContour ? 3.f : rule ? rule->lineWidth : 1.f);
            if (rule && rule->pattern != LinePattern::Solid) {
                glEnable(GL_LINE_STIPPLE);
                glLineStipple(1, rule->pattern == LinePattern::Dashed ? 0x0F0F : 0x1111);
            }
            for (const auto& line : f.lines)
                drawGeoPath(app, line, GL_LINE_STRIP);
            for (const auto& polygon : f.polygons)
                for (const auto& ring : polygon.rings)
                    drawGeoPath(app, ring, GL_LINE_LOOP);
            glDisable(GL_LINE_STIPPLE);
        }
        drawPoints(app, visible);
    }
    app.renderingCell = -1;
    drawDecorations(app);
    glDisable(GL_SCISSOR_TEST);
}

} // namespace enc::detail
