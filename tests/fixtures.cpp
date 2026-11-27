// fixtures.cpp — Generación de GeoTIFF de prueba con GDAL.
// Cubre RGB, alfa, grises de 16 bits, paletas, rotación, proyección, GCP y ubicación manual.
#include "test_support.h"
#include <gdal_priv.h>
#include <memory>
#include <ogrsf_frmts.h>
namespace enc_test {
// Escribe TIFF pequeños con distintos tipos de banda, proyecciones y georreferencias.
void fixture(const std::filesystem::path& path, int epsg, bool rotated, bool georeferenced,
             bool gray16, bool palette, bool gcps) {
    constexpr int width = 64, height = 48;
    auto* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    require(driver, "Missing TIFF driver");
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(
        driver->Create(path.string().c_str(), width, height, gray16 || palette ? 1 : 4,
                       gray16 ? GDT_UInt16 : GDT_Byte, nullptr),
        GDALClose);
    require(static_cast<bool>(ds), "Cannot create TIFF regression fixture");
    if (georeferenced) {
        OGRSpatialReference srs;
        srs.importFromEPSG(epsg);
        srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (gcps) {
            GDAL_GCP points[4];
            GDALInitGCPs(4, points);
            const double xy[][4] = {{0, 0, -71.06, 42.37},
                                    {64, 0, -71.02, 42.37},
                                    {64, 48, -71.02, 42.34},
                                    {0, 48, -71.06, 42.34}};
            for (int i = 0; i < 4; ++i) {
                points[i].dfGCPPixel = xy[i][0];
                points[i].dfGCPLine = xy[i][1];
                points[i].dfGCPX = xy[i][2];
                points[i].dfGCPY = xy[i][3];
            }
            require(ds->SetGCPs(4, points, &srs) == CE_None, "Cannot write TIFF control points");
            GDALDeinitGCPs(4, points);
        } else {
            double x = -71.06, y = 42.37;
            double pixelX = .04 / width, pixelY = -.03 / height;
            if (epsg == 3857) {
                OGRSpatialReference wgs84;
                wgs84.importFromEPSG(4326);
                wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                std::unique_ptr<OGRCoordinateTransformation,
                                decltype(&OCTDestroyCoordinateTransformation)>
                    transform(OGRCreateCoordinateTransformation(&wgs84, &srs),
                              OCTDestroyCoordinateTransformation);
                require(transform && transform->Transform(1, &x, &y),
                        "Cannot transform TIFF test origin");
                pixelX = 50;
                pixelY = -50;
            }
            double gt[] = {x,     pixelX, rotated ? pixelX * .25 : 0, y, rotated ? pixelY * .25 : 0,
                           pixelY};
            require(ds->SetGeoTransform(gt) == CE_None && ds->SetSpatialRef(&srs) == CE_None,
                    "Cannot georeference TIFF fixture");
        }
    }
    if (gray16) {
        std::vector<std::uint16_t> values(width * height);
        for (int i = 0; i < width * height; ++i)
            values[static_cast<std::size_t>(i)] = static_cast<std::uint16_t>(1000 + i);
        values[0] = 0;
        ds->GetRasterBand(1)->SetNoDataValue(0);
        require(ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, width, height, values.data(), width,
                                               height, GDT_UInt16, 0, 0, nullptr) == CE_None,
                "Cannot write gray TIFF");
    } else if (palette) {
        GDALColorTable table;
        GDALColorEntry red = {230, 20, 10, 255}, green = {10, 210, 30, 255};
        table.SetColorEntry(0, &red);
        table.SetColorEntry(1, &green);
        ds->GetRasterBand(1)->SetColorTable(&table);
        std::vector<unsigned char> values(width * height, 1);
        require(ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, width, height, values.data(), width,
                                               height, GDT_Byte, 0, 0, nullptr) == CE_None,
                "Cannot write palette TIFF");
    } else {
        for (int band = 1; band <= 4; ++band) {
            ds->GetRasterBand(band)->SetColorInterpretation(
                static_cast<GDALColorInterp>(GCI_RedBand + band - 1));
            const unsigned char colours[] = {230, 20, 10, 255};
            std::vector<unsigned char> values(width * height, colours[band - 1]);
            if (band == 4)
                for (int y = 0; y < 12; ++y)
                    for (int x = 0; x < 16; ++x)
                        values[static_cast<std::size_t>(y) * width + x] = 0;
            require(ds->GetRasterBand(band)->RasterIO(GF_Write, 0, 0, width, height, values.data(),
                                                      width, height, GDT_Byte, 0, 0,
                                                      nullptr) == CE_None,
                    "Cannot write RGBA TIFF");
        }
    }
}

} // namespace enc_test
