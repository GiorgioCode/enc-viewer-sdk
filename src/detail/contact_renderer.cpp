// contact_renderer.cpp — Símbolos AIS/radar, vectores, trazas, selección y avisos temporizados.
// Los contactos se dibujan después de TIFF/PPI; la orientación procede de heading y luego COG.
#include "contact_renderer.h"
#include "camera.h"
#include "text_renderer.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace enc::detail {
namespace {
void colour(unsigned rgb) {
    glColor3ub((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}
void line(double x1, double y1, double x2, double y2) {
    glBegin(GL_LINES);
    glVertex2d(x1, y1);
    glVertex2d(x2, y2);
    glEnd();
}
void circle(double radius, bool filled = false, double start = 0, double end = 360) {
    glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_STRIP);
    if (filled)
        glVertex2d(0, 0);
    for (int i = 0; i <= 48; ++i) {
        const double a = (start + (end - start) * i / 48) * kPi / 180;
        glVertex2d(radius * std::sin(a), -radius * std::cos(a));
    }
    glEnd();
}
double screenBearing(const SceneState& s, GeoPoint p, double bearing) {
    const auto a = toScreen(s, {p.lon, p.lat});
    const auto b = destination(p, bearing, .05);
    const auto end = toScreen(s, {b.lon, b.lat});
    return std::atan2(end.x - a.x, a.y - end.y) * 180 / kPi;
}
void corners(double radius) {
    for (int x : {-1, 1})
        for (int y : {-1, 1}) {
            line(x * radius, y * radius, x * (radius - 5), y * radius);
            line(x * radius, y * radius, x * radius, y * (radius - 5));
        }
}
void drawOutline(const SceneState& s, const Contact& c) {
    if (!c.dimensions || !c.heading)
        return;
    const auto& d = *c.dimensions;
    if ((d.bow + d.stern) <= 0 || (d.port + d.starboard) <= 0)
        return;
    if ((d.bow + d.stern) / metresPerPixel(s) < 24)
        return;
    glBegin(GL_LINE_LOOP);
    for (auto corner : std::array<std::pair<double, double>, 5>{{{0, d.bow},
                                                                 {d.starboard, d.bow * .7},
                                                                 {d.starboard, -d.stern},
                                                                 {-d.port, -d.stern},
                                                                 {-d.port, d.bow * .7}}}) {
        const double heading = *c.heading * kPi / 180;
        const double east = corner.first * std::cos(heading) + corner.second * std::sin(heading),
                     north = -corner.first * std::sin(heading) + corner.second * std::cos(heading);
        const auto g = destination(c.position, std::atan2(east, north) * 180 / kPi,
                                   std::hypot(east, north) / 1852);
        const auto p = toScreen(s, {g.lon, g.lat});
        glVertex2d(p.x, p.y);
    }
    glEnd();
}
} // namespace
void drawContacts(SceneState& s) {
    auto& state = s.contacts;
    const auto& settings = state.settings;
    if (!settings.visible)
        return;
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POINT_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LINE_STIPPLE);
    glLineWidth(1.5f);
    const unsigned normal = s.nightMode ? 0x51D69B : 0x006B44;
    if (state.ownShip) {
        const auto& own = *state.ownShip;
        const auto p = toScreen(s, {own.position.lon, own.position.lat});
        colour(normal);
        glPushMatrix();
        glTranslated(p.x, p.y, 0);
        circle(5);
        circle(9);
        glPopMatrix();
        if (own.heading) {
            const auto a = screenBearing(s, own.position, *own.heading) * kPi / 180;
            line(p.x, p.y, p.x + 45 * std::sin(a), p.y - 45 * std::cos(a));
        }
    }
    bool any = false, test = false;
    for (const auto& entry : state.records) {
        // El aviso global identifica el modo de prueba incluso con el blanco fuera
        // de pantalla o durante la fase apagada de una alarma (OMI, tabla 2).
        test |= entry.second.report.testTarget;
        const auto info = assessContact(state, entry.second);
        if (!info.visible)
            continue;
        const auto& c = info.report;
        const auto p = toScreen(s, {c.position.lon, c.position.lat});
        const double margin = std::max(100., settings.symbolScale * 60);
        if (p.x < -margin || p.y < -margin || p.x > s.width + margin || p.y > s.height + margin)
            continue;
        any = true;
        const bool lost = info.state == ContactState::Lost,
                   acquiring = info.state == ContactState::Acquiring;
        const bool alarm = info.dangerous || lost || (acquiring && c.automaticAcquisition);
        const bool flash = alarm && !info.acknowledged && std::fmod(state.seconds, 1.) >= .5;
        const bool acquisitionAlarm = acquiring && c.automaticAcquisition && !info.acknowledged;
        const unsigned rgb = (info.dangerous || acquisitionAlarm) ? 0xFF3030 : normal;
        // La selección y las consultas siguen disponibles durante la fase apagada del parpadeo.
        if (flash)
            continue;
        colour(rgb);
        const bool ais = c.source == ContactSource::AIS ||
                         (c.source == ContactSource::Associated && settings.associatedAsAIS);
        const bool sleeping = info.state == ContactState::Sleeping && !info.dangerous;
        const double scale = settings.symbolScale, size = sleeping ? 7 : 11;
        const bool oriented = c.heading || c.courseOverGround;
        const double angle =
            oriented
                ? screenBearing(s, c.position, c.heading.value_or(c.courseOverGround.value_or(0)))
                : 0;
        glLineWidth(info.dangerous || acquisitionAlarm ? 3.f : 1.5f);
        glPushMatrix();
        glTranslated(p.x, p.y, 0);
        glScaled(scale, scale, 1);
        if (ais) {
            glPushMatrix();
            glRotated(angle, 0, 0, 1);
            glBegin(info.dangerous ? GL_TRIANGLES : GL_LINE_LOOP);
            glVertex2d(0, -size);
            glVertex2d(size * .45, size);
            glVertex2d(-size * .45, size);
            glEnd();
            if (!lost && !sleeping && c.heading) {
                glLineWidth(1);
                line(0, -size, 0, -size * 5);
                if (c.rateOfTurn && std::abs(*c.rateOfTurn) > .01)
                    line(0, -size * 5, (*c.rateOfTurn > 0 ? 1 : -1) * 8, -size * 5);
            }
            glPopMatrix();
            if (!oriented)
                line(-8, 8, 8, -8);
            if (c.source == ContactSource::Associated)
                circle(size * 1.2);
        } else if (acquiring) {
            for (int start : {0, 120, 240})
                circle(8, false, start, start + 70);
        } else if (c.source == ContactSource::Associated) {
            // Alternativa radar de la tabla 4 OMI: círculo mayor con triángulo
            // inscrito. El contorno conserva el interior legible incluso en peligro.
            const double radius = size * 1.2, halfWidth = radius * .45;
            const double rear = std::sqrt(radius * radius - halfWidth * halfWidth);
            circle(radius);
            glPushMatrix();
            glRotated(angle, 0, 0, 1);
            glBegin(GL_LINE_LOOP);
            glVertex2d(0, -radius);
            glVertex2d(halfWidth, rear);
            glVertex2d(-halfWidth, rear);
            glEnd();
            glPopMatrix();
        } else
            circle(6, info.dangerous);
        if (lost) {
            glLineWidth(3);
            line(-10, -10, 10, 10);
            line(-10, 10, 10, -10);
        }
        if (info.selected) {
            glLineWidth(1.5);
            corners(17);
        }
        glPopMatrix();
        if (!lost && !sleeping && info.vectorEnd) {
            const auto end = toScreen(s, {info.vectorEnd->lon, info.vectorEnd->lat});
            glLineWidth(info.dangerous ? 2.5f : 1.5f);
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(1, 0x3333);
            line(p.x, p.y, end.x, end.y);
            glDisable(GL_LINE_STIPPLE);
        }
        if (ais && !lost && !sleeping && settings.showTrueOutline)
            drawOutline(s, c);
        if (!info.pastPositions.empty()) {
            glPointSize(3);
            glBegin(GL_POINTS);
            for (auto g : info.pastPositions) {
                const auto q = toScreen(s, {g.lon, g.lat});
                glVertex2d(q.x, q.y);
            }
            glEnd();
        }
        if ((settings.showNames && !sleeping) || info.selected) {
            const std::string label = c.name.empty() ? c.id : c.name;
            drawLabelText(s, p.x + 18 * scale, p.y - 12, label, rgb, .8);
        }
        if (c.testTarget) {
            drawLabelText(s, p.x + 15, p.y + 10, "X", rgb, 1);
        }
    }
    if (any) {
        std::ostringstream legend;
        legend << (settings.vectors == VectorMode::True ? "TRUE" : "RELATIVE") << " VECTORS / "
               << settings.vectorMinutes << " MIN / GROUND";
        if (!settings.showSleeping || settings.sleepingRangeNm > 0)
            legend << " / AIS FILTER";
        drawLabelText(s, 14, s.height - 75, legend.str(), normal, .8);
    }
    if (test)
        drawLabelText(s, s.width - 40, 16, "X", normal, 1.5);
    if (state.records.size() * 10 >= settings.capacity * 9)
        drawLabelText(s, 14, 34, "CONTACT CAPACITY", 0xFF3030, .9);
    glPopAttrib();
}
} // namespace enc::detail
