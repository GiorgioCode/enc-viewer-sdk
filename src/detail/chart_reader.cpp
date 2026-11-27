// chart_reader.cpp — Lectura privada de mundo y geometría ENC mediante GDAL.
// Transforma coordenadas, conserva atributos y copia geometrías; el catálogo público decide qué cargar.

#include "chart_reader.h"
#include "camera.h"
#include "chart_style.h"
#include "geometry.h"
#include <gdal_priv.h>
#include <memory>
#include <ogrsf_frmts.h>

namespace enc::detail {

namespace {
// Desenvuelve cada geometría respecto de una referencia común de celda antes de recortar y teselar.
void unwrapGeometry(OGRGeometry* geometry, double reference) {
    if (!geometry)
        return;
    const auto type = wkbFlatten(geometry->getGeometryType());
    if (type == wkbPoint) {
        auto* p = geometry->toPoint();
        p->setX(reference + wrapLongitude(p->getX() - reference));
    } else if (type == wkbLineString || type == wkbLinearRing) {
        auto* line = geometry->toLineString();
        for (int i = 0; i < line->getNumPoints(); ++i)
            line->setPoint(i, reference + wrapLongitude(line->getX(i) - reference), line->getY(i),
                           line->getZ(i));
    } else if (type == wkbPolygon) {
        auto* p = geometry->toPolygon();
        unwrapGeometry(p->getExteriorRing(), reference);
        for (int i = 0; i < p->getNumInteriorRings(); ++i)
            unwrapGeometry(p->getInteriorRing(i), reference);
    } else if (OGR_GT_IsSubClassOf(type, wkbGeometryCollection)) {
        auto* c = geometry->toGeometryCollection();
        for (int i = 0; i < c->getNumGeometries(); ++i)
            unwrapGeometry(c->getGeometryRef(i), reference);
    }
}

// Normaliza campos GDAL; las listas se guardan como códigos separados por comas.
std::string valueOf(OGRFeature& feature, int index) {
    if (!feature.IsFieldSetAndNotNull(index))
        return "<NULL>";
    const auto type = feature.GetFieldDefnRef(index)->GetType();
    if (type == OFTStringList) {
        std::string result;
        char** values = feature.GetFieldAsStringList(index);
        for (int i = 0; values && values[i]; ++i) {
            if (i)
                result += ',';
            result += values[i];
        }
        return result;
    }
    if (type == OFTIntegerList) {
        int count = 0;
        const int* values = feature.GetFieldAsIntegerList(index, &count);
        std::string result;
        for (int i = 0; i < count; ++i) {
            if (i)
                result += ',';
            result += std::to_string(values[i]);
        }
        return result;
    }
    const char* value = feature.GetFieldAsString(index);
    return value ? value : "";
}
} // namespace

// Carga los polígonos del mundo sin mezclarlos con las capas ni con los límites ENC.
bool loadBasemap(SceneState& app, const std::string& source) {
    if (source.empty())
        return false;
    auto* raw = static_cast<GDALDataset*>(
        GDALOpenEx(source.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr));
    if (!raw)
        return false;
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(raw, GDALClose);
    OGRLayer* layer = dataset->GetLayer(0);
    if (!layer)
        return false;
    std::vector<Feature> land;
    // Un estado temporal evita que el mundo altere el encuadre de las cartas cargadas.
    SceneState scratch;
    while (OGRFeature* rawFeature = layer->GetNextFeature()) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> source(
            rawFeature, OGRFeature::DestroyFeature);
        OGRGeometry* geometry = source->GetGeometryRef();
        if (!geometry || geometry->IsEmpty())
            continue;
        Feature feature;
        OGREnvelope bounds;
        geometry->getEnvelope(&bounds);
        feature.minX = bounds.MinX;
        feature.minY = bounds.MinY;
        feature.maxX = bounds.MaxX;
        feature.maxY = bounds.MaxY;
        appendGeometry(geometry, feature, scratch);
        if (!feature.polygons.empty())
            land.push_back(std::move(feature));
    }
    app.baseLand = std::move(land);
    return !app.baseLand.empty();
}

// Carga una celda S-57, transforma su geometría a WGS84 y conserva todos sus atributos.
bool addCell(SceneState& app, const fs::path& path) {
    const char* drivers[] = {"S57", nullptr};
    const char* options[] = {"UPDATES=APPLY", "RECODE_BY_DSSI=ON", "LNAM_REFS=ON", nullptr};
    auto* raw = static_cast<GDALDataset*>(GDALOpenEx(
        path.string().c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, options, nullptr));
    if (!raw)
        return false;
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(raw, GDALClose);
    OGRSpatialReference target;
    if (target.importFromEPSG(4326) != OGRERR_NONE)
        return false;
    // Fuerza longitud/latitud, evitando que EPSG:4326 invierta el orden de ejes.
    target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    const std::string cellName = path.filename().string();
    CellData cell;
    cell.path = path;
    if (auto* dsid = dataset->GetLayerByName("DSID")) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> f(
            dsid->GetNextFeature(), OGRFeature::DestroyFeature);
        if (f) {
            const auto number = [&](const char* name) {
                const int i = f->GetFieldIndex(name);
                return i < 0 ? 0 : f->GetFieldAsInteger(i);
            };
            cell.compilationScale = number("DSPM_CSCL");
            cell.intendedUsage = number("DSID_INTU");
        }
    }
    std::unique_ptr<OGRGeometry> mask, excluded;
    double reference = 0;
    bool referenceSet = false;
    if (auto* coverage = dataset->GetLayerByName("M_COVR")) {
        coverage->ResetReading();
        while (auto* rawFeature = coverage->GetNextFeature()) {
            std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> f(
                rawFeature, OGRFeature::DestroyFeature);
            auto* geometry = f->GetGeometryRef();
            if (!geometry || geometry->IsEmpty())
                continue;
            if (!referenceSet) {
                OGREnvelope e;
                geometry->getEnvelope(&e);
                reference = e.MaxX;
                referenceSet = true;
            }
            std::unique_ptr<OGRGeometry> g(geometry->clone());
            unwrapGeometry(g.get(), reference);
            auto& target = f->GetFieldAsInteger("CATCOV") == 1 ? mask : excluded;
            if (target)
                target.reset(target->Union(g.get()));
            else
                target = std::move(g);
        }
        if (mask && excluded)
            mask.reset(mask->Difference(excluded.get()));
        if (mask && !mask->IsEmpty()) {
            SceneState scratch;
            Feature coverageFeature;
            appendGeometry(mask.get(), coverageFeature, scratch);
            cell.coverage = std::move(coverageFeature.polygons);
            cell.hasCoverage = true;
        }
    }
    app.cells.push_back(std::move(cell));

    for (int layerIndex = 0; layerIndex < dataset->GetLayerCount(); ++layerIndex) {
        OGRLayer* sourceLayer = dataset->GetLayer(layerIndex);
        if (!sourceLayer)
            continue;
        Layer layer;
        layer.objectClass = sourceLayer->GetName() ? sourceLayer->GetName() : "(unnamed)";
        layer.cellKey = path.lexically_normal().string();
        layer.name = cellName + " / " + layer.objectClass;
        // Cobertura y calidad quedan disponibles en la lista, inicialmente ocultas como metadatos.
        layer.visible = layer.objectClass.compare(0, 2, "M_") != 0;
        app.layers.push_back(std::move(layer));
        const std::size_t layerIndexInApp = app.layers.size() - 1;

        std::unique_ptr<OGRCoordinateTransformation, decltype(&OCTDestroyCoordinateTransformation)>
            transform(nullptr, OCTDestroyCoordinateTransformation);
        if (sourceLayer->GetSpatialRef()) {
            OGRSpatialReference source(*sourceLayer->GetSpatialRef());
            source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            transform.reset(OGRCreateCoordinateTransformation(&source, &target));
            if (!transform)
                continue;
        }

        sourceLayer->ResetReading();
        while (OGRFeature* rawFeature = sourceLayer->GetNextFeature()) {
            std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> sourceFeature(
                rawFeature, OGRFeature::DestroyFeature);
            Feature feature;
            feature.layer = layerIndexInApp;
            feature.cell = app.cells.size() - 1;
            const OGRFeatureDefn* definition = sourceFeature->GetDefnRef();
            for (int field = 0; field < definition->GetFieldCount(); ++field) {
                feature.attributes.push_back({definition->GetFieldDefn(field)->GetNameRef(),
                                              valueOf(*sourceFeature, field)});
            }
            if (OGRGeometry* sourceGeometry = sourceFeature->GetGeometryRef()) {
                // Copia antes de transformar: la geometría original pertenece al objeto GDAL.
                std::unique_ptr<OGRGeometry> geometry(sourceGeometry->clone());
                if (transform && geometry->transform(transform.get()) != OGRERR_NONE)
                    continue;
                if (referenceSet)
                    unwrapGeometry(geometry.get(), reference);
                // Recorta a cobertura útil: conserva huecos y exclusiones CATCOV=2.
                if (mask && app.layers[layerIndexInApp].objectClass.rfind("M_", 0) != 0 &&
                    !mask->Contains(geometry.get())) {
                    geometry.reset(geometry->Intersection(mask.get()));
                    if (!geometry || geometry->IsEmpty()) {
                        app.features.push_back(std::move(feature));
                        ++app.layers[layerIndexInApp].count;
                        continue;
                    }
                }
                OGREnvelope bounds;
                geometry->getEnvelope(&bounds);
                feature.minX = bounds.MinX;
                feature.minY = bounds.MinY;
                feature.maxX = bounds.MaxX;
                feature.maxY = bounds.MaxY;
                appendGeometry(geometry.get(), feature, app);
                const auto kind = nautical::kindFor(app.layers[layerIndexInApp].objectClass);
                if (!feature.polygons.empty() && feature.points.empty() &&
                    (kind == nautical::Kind::Anchorage || kind == nautical::Kind::Marina ||
                     kind == nautical::Kind::Pilot)) {
                    // Las áreas con símbolo necesitan un anclaje interior, incluso si son cóncavas.
                    std::unique_ptr<OGRGeometry> point(OGRGeometry::FromHandle(
                        OGR_G_PointOnSurface(OGRGeometry::ToHandle(geometry.get()))));
                    if (point && !point->IsEmpty())
                        feature.points.push_back(
                            {point->toPoint()->getX(), point->toPoint()->getY()});
                }
            }
            app.features.push_back(std::move(feature));
            ++app.layers[layerIndexInApp].count;
        }
    }
    app.cells.back().bounds = {app.minLon, app.minLat, app.maxLon, app.maxLat, app.hasBounds};
    // Sin M_COVR utilizable se conserva una cobertura de respaldo explícita por envolvente.
    if (!app.cells.back().hasCoverage && app.hasBounds) {
        Polygon box;
        box.rings = {{{app.minLon, app.minLat},
                      {app.maxLon, app.minLat},
                      {app.maxLon, app.maxLat},
                      {app.minLon, app.maxLat}}};
        triangulate(box);
        app.cells.back().coverage.push_back(std::move(box));
        app.cells.back().hasCoverage = true;
    }
    app.indexedCells.push_back(path);
    return true;
}

} // namespace enc::detail
