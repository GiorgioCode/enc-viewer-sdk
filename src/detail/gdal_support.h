// gdal_support.h — Ayudas privadas para manejar errores y datasets GDAL con propiedad explícita.
// Captura mensajes en el hilo actual y limita la apertura de cartas al controlador S57.
#pragma once
#include <cpl_error.h>
#include <enc/types.h>
#include <gdal_priv.h>
#include <memory>
namespace enc::detail {
using Dataset = std::unique_ptr<GDALDataset, decltype(&GDALClose)>;
inline Dataset openCell(const std::filesystem::path& path) {
    const char* drivers[] = {"S57", nullptr};
    const char* options[] = {"UPDATES=APPLY", nullptr};
    return Dataset(static_cast<GDALDataset*>(GDALOpenEx(path.string().c_str(),
                                                        GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers,
                                                        options, nullptr)),
                   GDALClose);
}
class GdalMessages {
  public:
    std::vector<enc::Diagnostic> messages;
    std::filesystem::path path;
    explicit GdalMessages(std::filesystem::path source) : path(std::move(source)) {
        CPLPushErrorHandlerEx(
            [](CPLErr level, CPLErrorNum, const char* message) {
                auto* self = static_cast<GdalMessages*>(CPLGetErrorHandlerUserData());
                if (level >= CE_Warning)
                    self->messages.push_back(
                        {level >= CE_Failure ? enc::Severity::Error : enc::Severity::Warning,
                         self->path, message ? message : "Error GDAL"});
            },
            this);
    }
    ~GdalMessages() {
        CPLPopErrorHandler();
    }
    GdalMessages(const GdalMessages&) = delete;
    GdalMessages& operator=(const GdalMessages&) = delete;
};
inline enc::Status invalid(const std::string& message) {
    return {enc::ErrorCode::InvalidArgument, message, {}};
}
} // namespace enc::detail
