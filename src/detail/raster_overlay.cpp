// raster_overlay.cpp — Modelo y carga de imágenes TIFF y GeoTIFF.
// Normaliza bandas, transparencia y georreferencias; administra ubicación y textura OpenGL diferida.
// Contiene la implementación de esta responsabilidad.

#include "raster_overlay.h"
#include <algorithm>
#include <cmath>
#include <gdal_priv.h>
#include <gdalwarper.h>
#include <limits>
#include <ogrsf_frmts.h>

namespace enc::detail::overlays {
namespace {
constexpr int kMaxImageSize = 2048;
struct ReleaseDataset {
    void operator()(GDALDataset* ds) const {
        if (ds)
            GDALReleaseDataset(ds);
    }
};
using Dataset = std::unique_ptr<GDALDataset, ReleaseDataset>;
using Pixel = std::uint8_t;

// Lee y reduce una banda en CPU, conservando precisión antes de convertirla a bytes.
bool readBand(GDALRasterBand* band, int width, int height, std::vector<float>& values) {
    values.resize(static_cast<std::size_t>(width) * height);
    return band->RasterIO(GF_Read, 0, 0, band->GetXSize(), band->GetYSize(), values.data(), width,
                          height, GDT_Float32, 0, 0, nullptr) == CE_None;
}

// Normaliza RGB, grises y paletas antes de reproyectar: limita la memoria usada
// y entrega a GDAL un canal alfa explícito que también contempla los valores sin datos.
bool readRgba(GDALDataset& ds, int width, int height, std::vector<Pixel>& rgba,
              std::string& error) {
    const std::size_t count = static_cast<std::size_t>(width) * height;
    rgba.assign(count * 4, 255);
    int alpha = 0;
    std::vector<int> dataBands;
    std::array<int, 3> rgb{};
    // Respeta la interpretación de canales declarada por GDAL; contempla grises y paleta.
    for (int b = 1; b <= ds.GetRasterCount(); ++b) {
        switch (ds.GetRasterBand(b)->GetColorInterpretation()) {
        case GCI_AlphaBand:
            alpha = b;
            break;
        case GCI_RedBand:
            rgb[0] = b;
            dataBands.push_back(b);
            break;
        case GCI_GreenBand:
            rgb[1] = b;
            dataBands.push_back(b);
            break;
        case GCI_BlueBand:
            rgb[2] = b;
            dataBands.push_back(b);
            break;
        default:
            dataBands.push_back(b);
            break;
        }
    }
    if (dataBands.empty()) {
        error = "TIFF has no displayable bands.";
        return false;
    }
    GDALRasterBand* first = ds.GetRasterBand(dataBands.front());
    GDALColorTable* palette = first->GetColorTable();
    const bool trueRgb = rgb[0] && rgb[1] && rgb[2];
    if (!trueRgb)
        for (int c = 0; c < 3; ++c)
            rgb[c] =
                dataBands.size() >= 3 ? dataBands[static_cast<std::size_t>(c)] : dataBands.front();
    std::vector<Pixel> allNoData(count, 1);
    std::vector<float> values;
    for (int channel = 0; channel < (palette ? 1 : 3); ++channel) {
        auto* band = palette ? first : ds.GetRasterBand(rgb[channel]);
        if (!readBand(band, width, height, values)) {
            error = "Cannot read TIFF pixels.";
            return false;
        }
        int hasNoData = 0;
        const double noData = band->GetNoDataValue(&hasNoData);
        double minimum = std::numeric_limits<double>::infinity(), maximum = -minimum;
        for (std::size_t i = 0; i < count; ++i) {
            const double v = values[i];
            const bool invalid =
                !std::isfinite(v) ||
                (hasNoData && (v == noData || (std::isnan(v) && std::isnan(noData))));
            allNoData[i] = static_cast<Pixel>(allNoData[i] && invalid);
            if (invalid)
                continue;
            minimum = std::min(minimum, v);
            maximum = std::max(maximum, v);
        }
        // Las bandas no byte se ajustan al rango válido muestreado para hacerlas visibles.
        const bool stretch = band->GetRasterDataType() != GDT_Byte && !palette && maximum > minimum;
        for (std::size_t i = 0; i < count; ++i) {
            if (palette) {
                const auto* entry = std::isfinite(values[i])
                                        ? palette->GetColorEntry(static_cast<int>(values[i]))
                                        : nullptr;
                if (entry) {
                    rgba[4 * i] = static_cast<Pixel>(entry->c1);
                    rgba[4 * i + 1] = static_cast<Pixel>(entry->c2);
                    rgba[4 * i + 2] = static_cast<Pixel>(entry->c3);
                    rgba[4 * i + 3] = static_cast<Pixel>(entry->c4);
                } else
                    rgba[4 * i + 3] = 0;
            } else {
                const double v =
                    stretch ? (values[i] - minimum) * 255 / (maximum - minimum) : values[i];
                rgba[4 * i + channel] =
                    std::isfinite(v) ? static_cast<Pixel>(std::clamp(std::round(v), 0.0, 255.0))
                                     : 0;
            }
        }
    }
    if (alpha) {
        auto* band = ds.GetRasterBand(alpha);
        if (!readBand(band, width, height, values)) {
            error = "Cannot read TIFF alpha.";
            return false;
        }
        double maximum = 255;
        const auto type = band->GetRasterDataType();
        if (type == GDT_UInt16)
            maximum = 65535;
        else if (type == GDT_Int16)
            maximum = 32767;
        else if (type == GDT_Float32 || type == GDT_Float64) {
            float sampleMax = 0;
            for (float v : values)
                if (std::isfinite(v))
                    sampleMax = std::max(sampleMax, v);
            if (sampleMax <= 1)
                maximum = 1;
        }
        for (std::size_t i = 0; i < count; ++i)
            rgba[4 * i + 3] = std::isfinite(values[i])
                                  ? static_cast<Pixel>(rgba[4 * i + 3] *
                                                       std::clamp(values[i] / maximum, 0.0, 1.0))
                                  : 0;
    }
    // La máscara interna o externa puede invalidar píxeles además del alfa y los valores sin datos.
    const int maskFlags = first->GetMaskFlags();
    if ((maskFlags & GMF_PER_DATASET) && !(maskFlags & GMF_ALPHA)) {
        if (!readBand(first->GetMaskBand(), width, height, values)) {
            error = "Cannot read TIFF validity mask.";
            return false;
        }
        for (std::size_t i = 0; i < count; ++i)
            if (values[i] <= 0)
                rgba[4 * i + 3] = 0;
    }
    for (std::size_t i = 0; i < count; ++i)
        if (allNoData[i])
            rgba[4 * i + 3] = 0;
    return true;
}

// Escribe RGBA intercalado en las cuatro bandas de un dataset GDAL.
bool writeRgba(GDALDataset& ds, const std::vector<Pixel>& pixels, int width, int height) {
    int bands[] = {1, 2, 3, 4};
    return ds.RasterIO(GF_Write, 0, 0, width, height, const_cast<Pixel*>(pixels.data()), width,
                       height, GDT_Byte, 4, bands, 4, static_cast<GSpacing>(width) * 4, 1,
                       nullptr) == CE_None;
}
} // namespace

// Sólo collect/release y la construcción de textura necesitan contexto; los destructores no.
void GpuResources::collect() {
    for (auto id : retired)
        if (live.erase(id))
            glDeleteTextures(1, &id);
    retired.clear();
}
void GpuResources::release() {
    for (auto id : live)
        glDeleteTextures(1, &id);
    live.clear();
    retired.clear();
}
Texture::Texture(std::shared_ptr<GpuResources> resources) : owner(std::move(resources)) {
    glGenTextures(1, &id);
    if (id)
        owner->live.insert(id);
}
Texture::~Texture() {
    if (id && owner->live.count(id))
        owner->retired.push_back(id);
}

// Calcula el centro original: la edición manual se aplica sin modificar las esquinas fuente.
GeoPoint Raster::centre() const {
    GeoPoint result;
    for (const auto& p : corners) {
        result.lon += p.lon * .25;
        result.lat += p.lat * .25;
    }
    return result;
}
// Aplica escala y desplazamiento geográfico a una esquina para el dibujado.
GeoPoint Raster::placedCorner(std::size_t i) const {
    const auto c = centre();
    return {c.lon + (corners[i].lon - c.lon) * scale + offsetLon,
            c.lat + (corners[i].lat - c.lat) * scale + offsetLat};
}
// Redimensiona manteniendo fijo el punto geográfico elegido por el cursor.
void Raster::scaleAt(GeoPoint anchor, double factor) {
    const double next = std::clamp(scale * factor, .01, 100.0);
    factor = next / scale;
    const auto c = centre();
    offsetLon = (1 - factor) * (anchor.lon - c.lon) + factor * offsetLon;
    offsetLat = (1 - factor) * (anchor.lat - c.lat) + factor * offsetLat;
    scale = next;
}
// Sube la imagen una sola vez; requiere un contexto vigente y reutiliza la textura después.
void Raster::upload(const std::shared_ptr<GpuResources>& resources) {
    if (texture || !loaded())
        return;
    texture = std::make_shared<Texture>(resources);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Prepara una imagen temporal y reemplaza la anterior sólo cuando toda la carga es válida.
bool loadRaster(Raster& result, const std::filesystem::path& path,
                const std::array<GeoPoint, 4>& fallbackExtent, std::string& error) {
    error.clear();
    Dataset source(static_cast<GDALDataset*>(GDALOpenEx(
        path.string().c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, nullptr, nullptr)));
    if (!source || source->GetRasterCount() < 1 || source->GetRasterXSize() < 1 ||
        source->GetRasterYSize() < 1) {
        error = "Cannot open TIFF image: " + path.filename().string();
        return false;
    }
    auto* driver = source->GetDriver();
    if (!driver || std::string(driver->GetDescription()) != "GTiff") {
        error = "Choose a TIFF/GeoTIFF file.";
        return false;
    }
    Raster image;
    image.path = path;
    // Limita memoria y tamaño de textura: se visualiza una previsualización de hasta 2048 píxeles.
    const double reduction =
        std::min(1.0, static_cast<double>(kMaxImageSize) /
                          std::max(source->GetRasterXSize(), source->GetRasterYSize()));
    image.width = std::max(1, static_cast<int>(source->GetRasterXSize() * reduction));
    image.height = std::max(1, static_cast<int>(source->GetRasterYSize() * reduction));
    if (!readRgba(*source, image.width, image.height, image.rgba, error))
        return false;
    double gt[6];
    const bool affine = source->GetGeoTransform(gt) == CE_None && source->GetSpatialRef();
    const bool gcps = source->GetGCPCount() >= 3 && source->GetGCPSpatialRef();
    if (!affine && !gcps) {
        image.corners = fallbackExtent;
        // Conserva la proporción de píxeles al ubicar una imagen sin referencia geográfica.
        const auto c = image.centre();
        const double cosLat = std::max(.17, std::cos(c.lat * 3.14159265358979323846 / 180));
        const double lonWidth = std::abs(image.corners[1].lon - image.corners[0].lon);
        const double latHeight = lonWidth * cosLat * image.height / image.width;
        for (int i = 0; i < 4; ++i)
            image.corners[static_cast<std::size_t>(i)].lat = c.lat + (i < 2 ? .5 : -.5) * latHeight;
        result = std::move(image);
        return true;
    }

    auto* mem = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!mem) {
        error = "GDAL MEM raster driver is unavailable.";
        return false;
    }
    Dataset normalized(mem->Create("", image.width, image.height, 4, GDT_Byte, nullptr));
    if (!normalized || !writeRgba(*normalized, image.rgba, image.width, image.height)) {
        error = "Cannot prepare TIFF reprojection.";
        return false;
    }
    for (int b = 1; b <= 4; ++b)
        normalized->GetRasterBand(b)->SetColorInterpretation(
            static_cast<GDALColorInterp>(GCI_RedBand + b - 1));
    const double sx = static_cast<double>(source->GetRasterXSize()) / image.width;
    const double sy = static_cast<double>(source->GetRasterYSize()) / image.height;
    // Ajusta la georreferencia a la resolución reducida, conservando rotación y puntos de control.
    if (affine) {
        gt[1] *= sx;
        gt[4] *= sx;
        gt[2] *= sy;
        gt[5] *= sy;
        normalized->SetGeoTransform(gt);
        normalized->SetSpatialRef(source->GetSpatialRef());
    } else {
        GDAL_GCP* points = GDALDuplicateGCPs(source->GetGCPCount(), source->GetGCPs());
        for (int i = 0; i < source->GetGCPCount(); ++i) {
            points[i].dfGCPPixel /= sx;
            points[i].dfGCPLine /= sy;
        }
        const auto status =
            normalized->SetGCPs(source->GetGCPCount(), points, source->GetGCPSpatialRef());
        GDALDeinitGCPs(source->GetGCPCount(), points);
        CPLFree(points);
        if (status != CE_None) {
            error = "Cannot copy TIFF ground control points.";
            return false;
        }
    }
    OGRSpatialReference wgs84;
    if (wgs84.importFromEPSG(4326) != OGRERR_NONE) {
        error = "Cannot initialize WGS84.";
        return false;
    }
    char* wkt = nullptr;
    wgs84.exportToWkt(&wkt);
    std::unique_ptr<GDALWarpOptions, decltype(&GDALDestroyWarpOptions)> options(
        GDALCreateWarpOptions(), GDALDestroyWarpOptions);
    options->nSrcAlphaBand = 4;
    options->nDstAlphaBand = 4;
    options->nBandCount = 3;
    options->panSrcBands = static_cast<int*>(CPLMalloc(3 * sizeof(int)));
    options->panDstBands = static_cast<int*>(CPLMalloc(3 * sizeof(int)));
    for (int i = 0; i < 3; ++i)
        options->panSrcBands[i] = options->panDstBands[i] = i + 1;
    // El VRT mantiene una referencia al dataset normalizado; se destruye antes que éste.
    // La reproyección usa alfa separado para evitar bordes opacos en zonas sin datos.
    Dataset warped(static_cast<GDALDataset*>(
        GDALAutoCreateWarpedVRT(normalized.get(), nullptr, wkt, GRA_Bilinear, .1, options.get())));
    CPLFree(wkt);
    if (!warped || warped->GetGeoTransform(gt) != CE_None || warped->GetRasterCount() < 4) {
        error = "Cannot reproject TIFF to WGS84.";
        return false;
    }
    const int w = warped->GetRasterXSize(), h = warped->GetRasterYSize();
    if (w < 1 || h < 1) {
        error = "TIFF has an invalid geographic footprint.";
        return false;
    }
    const double resample = std::min(1.0, static_cast<double>(kMaxImageSize) / std::max(w, h));
    image.width = std::max(1, static_cast<int>(w * resample));
    image.height = std::max(1, static_cast<int>(h * resample));
    image.rgba.resize(static_cast<std::size_t>(image.width) * image.height * 4);
    int bands[] = {1, 2, 3, 4};
    if (warped->RasterIO(GF_Read, 0, 0, w, h, image.rgba.data(), image.width, image.height,
                         GDT_Byte, 4, bands, 4, static_cast<GSpacing>(image.width) * 4, 1,
                         nullptr) != CE_None) {
        error = "Cannot read reprojected TIFF.";
        return false;
    }
    // Usa bordes exteriores de píxel para que textura y huella geográfica coincidan.
    const double pixels[][2] = {{0, 0},
                                {static_cast<double>(w), 0},
                                {static_cast<double>(w), static_cast<double>(h)},
                                {0, static_cast<double>(h)}};
    for (int i = 0; i < 4; ++i) {
        image.corners[i] = {gt[0] + pixels[i][0] * gt[1] + pixels[i][1] * gt[2],
                            gt[3] + pixels[i][0] * gt[4] + pixels[i][1] * gt[5]};
        const auto p = image.corners[i];
        if (!std::isfinite(p.lon) || !std::isfinite(p.lat) || std::abs(p.lat) > 90.001 ||
            std::abs(p.lon) > 180.001) {
            error =
                "TIFF footprint crosses a pole or the date line; split the raster before loading.";
            return false;
        }
    }
    if (std::abs(image.corners[1].lon - image.corners[0].lon) > 180) {
        error = "TIFF crosses the date line; split it before loading.";
        return false;
    }
    image.georeferenced = true;
    result = std::move(image);
    return true;
}

// Genera una imagen sintética georreferenciada con retícula y transparencia para demostraciones.
bool createDemoTiff(const std::filesystem::path& path, const std::array<GeoPoint, 4>& extent,
                    std::string& error) {
    auto* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!driver) {
        error = "GDAL TIFF driver is unavailable.";
        return false;
    }
    constexpr int size = 384;
    Dataset ds(driver->Create(path.string().c_str(), size, size, 4, GDT_Byte, nullptr));
    if (!ds) {
        error = "Cannot create demo TIFF.";
        return false;
    }
    double gt[] = {extent[0].lon,
                   (extent[1].lon - extent[0].lon) / size,
                   0,
                   extent[0].lat,
                   0,
                   (extent[3].lat - extent[0].lat) / size};
    OGRSpatialReference srs;
    srs.importFromEPSG(4326);
    ds->SetGeoTransform(gt);
    ds->SetSpatialRef(&srs);
    std::vector<Pixel> pixels(static_cast<std::size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const double dx = (x - size * .5) / (size * .5), dy = (y - size * .5) / (size * .5);
            const double wave = .5 + .5 * std::sin(8 * dx + 3 * std::cos(6 * dy));
            const bool grid = x % 48 < 2 || y % 48 < 2;
            const bool border = x < 3 || y < 3 || x >= size - 3 || y >= size - 3;
            const std::size_t i = (static_cast<std::size_t>(y) * size + x) * 4;
            pixels[i] = static_cast<Pixel>(grid || border ? 255 : 235 * wave);
            pixels[i + 1] = static_cast<Pixel>(grid || border ? 240 : 110 + 100 * (1 - wave));
            pixels[i + 2] = static_cast<Pixel>(grid || border ? 255 : 210 * (1 - wave));
            pixels[i + 3] = static_cast<Pixel>(grid || border ? 240 : 170);
        }
    for (int b = 1; b <= 4; ++b)
        ds->GetRasterBand(b)->SetColorInterpretation(
            static_cast<GDALColorInterp>(GCI_RedBand + b - 1));
    if (!writeRgba(*ds, pixels, size, size)) {
        error = "Cannot write demo TIFF.";
        return false;
    }
    error.clear();
    return true;
}
} // namespace enc::detail::overlays
