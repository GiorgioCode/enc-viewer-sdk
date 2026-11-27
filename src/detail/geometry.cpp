// geometry.cpp — Conversión y operaciones geométricas del visor.
// Copia geometrías OGR, tesela superficies con huecos y ofrece consultas para selección y límites.
// Contiene la implementación de esta responsabilidad.

#include "geometry.h"
#include <GL/glu.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <ogrsf_frmts.h>

namespace enc::detail {

namespace {
// Datos temporales que deben sobrevivir a todos los callbacks de una teselación.
struct TessContext {
    Polygon* polygon = nullptr;
    GLenum primitive = 0;
    std::vector<Point*> vertices;
    std::vector<std::unique_ptr<Point>> generated;
};

// Inicia un lote de primitivas generado por el teselador GLU.
void tessBegin(GLenum primitive, void* polygonData) {
    auto* context = static_cast<TessContext*>(polygonData);
    context->primitive = primitive;
    context->vertices.clear();
}

// Conserva las referencias de vértices hasta completar la primitiva actual.
void tessVertex(void* vertexData, void* polygonData) {
    static_cast<TessContext*>(polygonData)->vertices.push_back(static_cast<Point*>(vertexData));
}

// Convierte triángulos, abanicos y tiras de GLU a una lista uniforme de triángulos.
void tessEnd(void* polygonData) {
    auto* context = static_cast<TessContext*>(polygonData);
    auto& vertices = context->vertices;
    auto& triangles = context->polygon->triangles;
    const auto add = [&vertices, &triangles](std::size_t a, std::size_t b, std::size_t c) {
        triangles.push_back(*vertices[a]);
        triangles.push_back(*vertices[b]);
        triangles.push_back(*vertices[c]);
    };
    if (context->primitive == GL_TRIANGLES) {
        for (std::size_t i = 0; i + 3 <= vertices.size(); i += 3)
            add(i, i + 1, i + 2);
    } else if (context->primitive == GL_TRIANGLE_FAN) {
        for (std::size_t i = 2; i < vertices.size(); ++i)
            add(0, i - 1, i);
    } else if (context->primitive == GL_TRIANGLE_STRIP) {
        for (std::size_t i = 2; i < vertices.size(); ++i) {
            if (i % 2 == 0)
                add(i - 2, i - 1, i);
            else
                add(i - 1, i - 2, i);
        }
    }
    vertices.clear();
}

// Mantiene vivos los vértices nuevos que GLU crea al resolver intersecciones.
void tessCombine(GLdouble coords[3], void* [4], GLfloat[4], void** output, void* polygonData) {
    auto* context = static_cast<TessContext*>(polygonData);
    auto point = std::make_unique<Point>(Point{coords[0], coords[1]});
    output[0] = point.get();
    context->generated.push_back(std::move(point));
}

// Copia una línea OGR y registra sus coordenadas en los límites de la carta.
void appendLine(OGRLineString* line, Feature& feature, SceneState& app) {
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(line->getNumPoints()));
    for (int i = 0; i < line->getNumPoints(); ++i) {
        points.push_back({line->getX(i), line->getY(i)});
        extendBounds(app, line->getX(i), line->getY(i));
    }
    if (points.size() == 1)
        feature.points.push_back(points.front());
    else if (!points.empty())
        feature.lines.push_back(std::move(points));
}

} // namespace

// Amplía la extensión de las cartas con coordenadas válidas; el mapa mundial no participa.
void extendBounds(SceneState& app, double lon, double lat) {
    if (!std::isfinite(lon) || !std::isfinite(lat))
        return;
    if (!app.hasBounds) {
        app.minLon = app.maxLon = lon;
        app.minLat = app.maxLat = lat;
        app.hasBounds = true;
        return;
    }
    app.minLon = std::min(app.minLon, lon);
    app.maxLon = std::max(app.maxLon, lon);
    app.minLat = std::min(app.minLat, lat);
    app.maxLat = std::max(app.maxLat, lat);
}

// Tesela contornos y huecos una sola vez durante la carga, sin rellenar los huecos.
void triangulate(Polygon& polygon) {
    if (polygon.rings.empty())
        return;
    GLUtesselator* tess = gluNewTess();
    if (!tess)
        return;
    TessContext context;
    context.polygon = &polygon;
    gluTessCallback(tess, GLU_TESS_BEGIN_DATA, reinterpret_cast<_GLUfuncptr>(tessBegin));
    gluTessCallback(tess, GLU_TESS_VERTEX_DATA, reinterpret_cast<_GLUfuncptr>(tessVertex));
    gluTessCallback(tess, GLU_TESS_END_DATA, reinterpret_cast<_GLUfuncptr>(tessEnd));
    gluTessCallback(tess, GLU_TESS_COMBINE_DATA, reinterpret_cast<_GLUfuncptr>(tessCombine));
    gluTessNormal(tess, 0, 0, 1);
    // La regla impar conserva los huecos sin depender de la orientación de sus anillos.
    gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
    gluTessBeginPolygon(tess, &context);
    // Las direcciones entregadas a GLU deben mantenerse estables hasta EndPolygon.
    std::vector<std::vector<std::array<GLdouble, 3>>> coords;
    coords.reserve(polygon.rings.size());
    for (const auto& ring : polygon.rings) {
        if (ring.size() < 3)
            continue;
        coords.emplace_back();
        auto& ringCoords = coords.back();
        const std::size_t count =
            ring.size() > 1 && ring.front().x == ring.back().x && ring.front().y == ring.back().y
                ? ring.size() - 1
                : ring.size();
        ringCoords.reserve(count);
        gluTessBeginContour(tess);
        for (std::size_t i = 0; i < count; ++i) {
            ringCoords.push_back({ring[i].x, ring[i].y, 0});
            gluTessVertex(tess, ringCoords.back().data(), const_cast<Point*>(&ring[i]));
        }
        gluTessEndContour(tess);
    }
    gluTessEndPolygon(tess);
    gluDeleteTess(tess);
}

// Recorre puntos, líneas, polígonos y colecciones OGR, conservando las profundidades Z.
void appendGeometry(OGRGeometry* geometry, Feature& feature, SceneState& app) {
    if (!geometry)
        return;
    switch (wkbFlatten(geometry->getGeometryType())) {
    case wkbPoint: {
        const auto* point = geometry->toPoint();
        if (point->IsEmpty())
            break;
        feature.points.push_back(
            {point->getX(), point->getY(),
             point->Is3D() ? point->getZ() : std::numeric_limits<double>::quiet_NaN()});
        extendBounds(app, point->getX(), point->getY());
        break;
    }
    case wkbLineString:
    case wkbLinearRing:
        appendLine(geometry->toLineString(), feature, app);
        break;
    case wkbPolygon: {
        auto* source = geometry->toPolygon();
        Polygon polygon;
        for (int r = 0; r <= source->getNumInteriorRings(); ++r) {
            OGRLinearRing* sourceRing =
                r == 0 ? source->getExteriorRing() : source->getInteriorRing(r - 1);
            if (!sourceRing)
                continue;
            std::vector<Point> ring;
            ring.reserve(static_cast<std::size_t>(sourceRing->getNumPoints()));
            for (int i = 0; i < sourceRing->getNumPoints(); ++i) {
                ring.push_back({sourceRing->getX(i), sourceRing->getY(i)});
                extendBounds(app, sourceRing->getX(i), sourceRing->getY(i));
            }
            polygon.rings.push_back(std::move(ring));
        }
        triangulate(polygon);
        feature.polygons.push_back(std::move(polygon));
        break;
    }
    case wkbMultiPoint:
    case wkbMultiLineString:
    case wkbMultiPolygon:
    case wkbGeometryCollection: {
        auto* collection = geometry->toGeometryCollection();
        for (int i = 0; i < collection->getNumGeometries(); ++i)
            appendGeometry(collection->getGeometryRef(i), feature, app);
        break;
    }
    default:
        break;
    }
}

// Evalúa si un punto está dentro de un anillo con el algoritmo de cruce de rayos.
bool inRing(Point p, const std::vector<Point>& ring) {
    if (ring.size() < 3)
        return false;
    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const Point a = ring[i], b = ring[j];
        if (((a.y > p.y) != (b.y > p.y)) &&
            p.x < (b.x - a.x) * (p.y - a.y) / ((b.y - a.y) == 0 ? 1e-20 : b.y - a.y) + a.x)
            inside = !inside;
    }
    return inside;
}

// Calcula la distancia de un punto a un segmento, incluidos los segmentos degenerados.
double segmentDistance(Point p, Point a, Point b) {
    const double dx = b.x - a.x, dy = b.y - a.y, len = dx * dx + dy * dy;
    const double t =
        len == 0 ? 0 : std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len, 0.0, 1.0);
    return std::hypot(p.x - a.x - t * dx, p.y - a.y - t * dy);
}

} // namespace enc::detail
