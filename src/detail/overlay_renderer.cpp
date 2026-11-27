// overlay_renderer.cpp — Composición gráfica de las superposiciones.
// Dibuja primero el TIFF y después el PPI, recortados al mapa y adaptados al modo día o noche.
// Contiene la implementación de esta responsabilidad.

#include "overlay_renderer.h"
#include "camera.h"
#include "render_primitives.h"
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace enc::detail {

namespace {

// Dibuja el TIFF sobre la carta con su alfa, ubicación y atenuación nocturna.
void drawRaster(SceneState& app, overlays::Raster& image) {
    if (!image.loaded() || !image.visible || image.opacity <= 0)
        return;
    // Una huella sobre ±180° mantiene una rama común; cada copia se recorta antes de dibujar.
    std::array<Point, 4> corners;
    const double anchor = image.placedCorner(0).lon;
    for (int i = 0; i < 4; ++i) {
        const auto point = image.placedCorner(i);
        corners[i] = {anchor + wrapLongitude(point.lon - anchor), point.lat};
    }
    std::vector<std::array<Point, 4>> copies;
    for (int copy = -1; copy <= 1; ++copy) {
        std::array<Point, 4> projected;
        double left = 1e30, right = -1e30, top = 1e30, bottom = -1e30;
        for (int i = 0; i < 4; ++i) {
            projected[i] = toScreenContinuous(app, corners[i], anchor, copy);
            left = std::min(left, projected[i].x);
            right = std::max(right, projected[i].x);
            top = std::min(top, projected[i].y);
            bottom = std::max(bottom, projected[i].y);
        }
        if (right >= -1 && left <= app.width + 1 && bottom >= -1 && top <= app.height + 1)
            copies.push_back(projected);
    }
    if (copies.empty())
        return;
    image.upload(app.gpu);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, image.texture->id);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    // Atenúa la muestra al dibujar: los píxeles fuente permanecen disponibles para el modo diurno.
    const double brightness = app.nightMode ? .35 : 1.0;
    glColor4d(brightness, brightness, brightness, image.opacity);
    constexpr double uv[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    glBegin(GL_QUADS);
    for (const auto& projected : copies)
        for (int i = 0; i < 4; ++i) {
            glTexCoord2dv(uv[i]);
            glVertex2d(projected[i].x, projected[i].y);
        }
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    setHex(0xE9A84C, .75f);
    glLineWidth(1);
    for (const auto& projected : copies) {
        glBegin(GL_LINE_LOOP);
        for (auto point : projected)
            glVertex2d(point.x, point.y);
        glEnd();
    }
}

// Usa fósforo verde de día y rojo de noche, conservando la intensidad y el alfa.
void radarColour(const SceneState& app, double r, double g, double b, double alpha) {
    if (app.nightMode)
        glColor4d(g, .06 * g, .035 * g, alpha);
    else
        glColor4d(r, g, b, alpha);
}

// Compone disco, barrido, anillos, ecos persistentes y blancos en coordenadas del radar.
void drawRadar(const SceneState& app) {
    const auto& radar = app.radar;
    if (!radar.visible || !radar.initialized || radar.opacity <= 0)
        return;
    const auto centre = toScreen(app, {radar.origin.lon, radar.origin.lat});
    const double rx = radar.rangeNm / (60 * radar.cosine()) * lonScale(app) * app.zoom,
                 ry = radar.rangeNm / 60 * app.zoom *
                      (app.projection == Projection::Mercator ? 1 / radar.cosine() : 1);
    if (rx < 3 || ry < 3)
        return;
    const auto [sx, sy] = framebufferScale(app);
    (void)sy;
    glPushMatrix();
    glTranslated(centre.x, centre.y, 0);
    glRotated(app.rotation, 0, 0, 1);
    glScaled(rx, ry, 1);
    radarColour(app, 0, .025, .012, radar.opacity * .75);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2d(0, 0);
    for (int i = 0; i <= 180; ++i) {
        const double a = i * 2 * kPi / 180;
        glVertex2d(std::sin(a), -std::cos(a));
    }
    glEnd();
    // El sector queda detrás de los retornos para conservar legibles los ecos persistentes.
    const double sweep = radar.sweep();
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 40; ++i) {
        const double a = sweep - (40 - i) * .012, b = a + .012;
        radarColour(app, .05, .95, .25, radar.opacity * .16 * i / 40);
        glVertex2d(0, 0);
        glVertex2d(std::sin(a), -std::cos(a));
        glVertex2d(std::sin(b), -std::cos(b));
    }
    glEnd();
    radarColour(app, .15, .8, .35, radar.opacity * .42);
    glLineWidth(1);
    for (int ring = 1; ring <= 4; ++ring) {
        const double r = ring * .25;
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 180; ++i) {
            const double a = i * 2 * kPi / 180;
            glVertex2d(r * std::sin(a), -r * std::cos(a));
        }
        glEnd();
    }
    glBegin(GL_LINES);
    for (int degree = 0; degree < 360; degree += 10) {
        const double angle = degree * kPi / 180, inner = degree % 30 == 0 ? .94 : .975;
        glVertex2d(inner * std::sin(angle), -inner * std::cos(angle));
        glVertex2d(std::sin(angle), -std::cos(angle));
    }
    glVertex2d(-.04, 0);
    glVertex2d(.04, 0);
    glVertex2d(0, -.04);
    glVertex2d(0, .04);
    glEnd();
    glEnable(GL_POINT_SMOOTH);
    // Dos tamaños de punto separan el halo tenue del núcleo brillante de cada eco costero.
    for (int pass = 0; pass < 2; ++pass) {
        glPointSize(static_cast<float>((pass ? 2.4 : 5.5) * sx));
        glBegin(GL_POINTS);
        for (const auto& echo : radar.coast) {
            const double brightness =
                std::clamp((radar.external ? 1.0 : radar.persistence(echo.point)) * echo.intensity *
                               radar.gain,
                           0.0, 1.0);
            radarColour(app, .08, 1, .22, radar.opacity * brightness * (pass ? .9 : .18));
            glVertex2d(echo.point.east / radar.rangeNm, -echo.point.north / radar.rangeNm);
        }
        glEnd();
    }
    glPointSize(static_cast<float>(1.6 * sx));
    glBegin(GL_POINTS);
    // Distribución determinista de ruido: añade clutter sin introducir azar en las pruebas.
    for (int i = 0; !radar.external && i < 650; ++i) {
        const double angle = i * 2.3999632297,
                     r = std::sqrt(std::fmod(i * .61803398875 + .17, 1.0));
        const overlays::LocalPoint p{r * std::sin(angle) * radar.rangeNm,
                                     r * std::cos(angle) * radar.rangeNm};
        const double noise = .15 + .1 * std::sin(i * 7.0 + std::floor(radar.seconds * 2));
        radarColour(app, .1, .85, .25,
                    radar.opacity * radar.gain * noise * radar.persistence(p) * (.9 - r * .65));
        glVertex2d(p.east / radar.rangeNm, -p.north / radar.rangeNm);
    }
    glEnd();
    glPointSize(static_cast<float>(7 * sx));
    glBegin(GL_POINTS);
    // Consulta posiciones pasadas del mismo movimiento para dibujar estelas breves.
    for (std::size_t i = 0; !radar.external && i < radar.targets.size(); ++i)
        for (int trail = 4; trail >= 0; --trail) {
            const auto p = radar.targetAt(i, std::max(0.0, radar.seconds - trail * .8));
            if (std::hypot(p.east, p.north) > radar.rangeNm)
                continue;
            radarColour(app, .45, 1, .3,
                        radar.opacity * radar.persistence(p) * radar.gain * (trail ? .16 : .95));
            glVertex2d(p.east / radar.rangeNm, -p.north / radar.rangeNm);
        }
    glEnd();
    glDisable(GL_POINT_SMOOTH);
    glLineWidth(1.5f);
    radarColour(app, .3, 1, .4, radar.opacity * .95);
    glBegin(GL_LINES);
    glVertex2d(0, 0);
    glVertex2d(std::sin(sweep), -std::cos(sweep));
    glEnd();
    glPopMatrix();
    // Las leyendas conservan texto horizontal, pero su posición sigue el norte geográfico del PPI.
    const double angle = app.rotation * kPi / 180, c = std::cos(angle), sn = std::sin(angle);
    drawText(centre.x + (ry + 12) * sn - 3, centre.y - (ry + 12) * c, "N", 0x2A9A59);
    for (int ring = 1; ring <= 4; ++ring) {
        std::ostringstream text;
        text << std::fixed << std::setprecision(2) << radar.rangeNm * ring / 4 << " NM";
        drawText(centre.x + ry * ring / 4 * sn + 6 * c, centre.y - ry * ring / 4 * c + 6 * sn + 4,
                 text.str(), 0x279456);
    }
    drawText(14, kToolbarHeight + 14,
             radar.external                  ? "RADAR PPI / EXTERNAL ECHOES"
             : radar.paused                  ? "RADAR PPI / SIMULATED / PAUSED"
             : std::abs(app.rotation) > 1e-8 ? "RADAR PPI / SIMULATED / GEOREFERENCED"
                                             : "RADAR PPI / SIMULATED / NORTH UP",
             0x25874C);
    if (radar.placingOrigin)
        drawText(14, kToolbarHeight + 29, "CLICK THE CHART TO SET RADAR ORIGIN", 0xC56F29);
}

} // namespace

// Recorta las superposiciones al mapa y compone primero el TIFF y después el PPI.
void drawOverlays(SceneState& app) {
    const auto [sx, sy] = framebufferScale(app);
    glEnable(GL_SCISSOR_TEST);
    glScissor(app.viewportX, app.viewportY + static_cast<int>(kStatusHeight * sy),
              static_cast<int>(std::max(1, app.width - kSidebarWidth) * sx),
              static_cast<int>(std::max(1, app.height - kToolbarHeight - kStatusHeight) * sy));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (auto& layer : app.rasters)
        drawRaster(app, layer.image);
    drawRadar(app);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
}

} // namespace enc::detail
