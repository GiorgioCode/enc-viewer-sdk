// streaming.h — Carga progresiva de celdas por región, con caché CPU y confirmación explícita.
// El trabajador nunca toca el Viewer ni OpenGL; poll confirma la escena desde el hilo anfitrión.
#pragma once
#include "viewer.h"
namespace enc {
struct StreamOptions {
    std::size_t cacheBudgetBytes = 128 * 1024 * 1024, maxCells = 256;
    bool allowPartial = false;
};
struct StreamInfo {
    bool loading = false;
    std::uint64_t revision = 0;
    std::size_t completed = 0, total = 0, cacheBytes = 0, cacheEntries = 0;
    std::size_t cacheHits = 0, cacheMisses = 0;
    Status lastResult;
};
class ChartStreamer {
  public:
    // Copia la instantánea del catálogo; opciones inválidas producen std::invalid_argument.
    explicit ChartStreamer(const Catalog& catalog, const StreamOptions& options = {});
    ~ChartStreamer(); // Cancela y espera el GDAL en curso antes de destruir el trabajador.
    ChartStreamer(const ChartStreamer&) = delete;
    ChartStreamer& operator=(const ChartStreamer&) = delete;
    Status requestView(const Bounds& region, double scaleDenominator = 0);
    bool poll(Viewer& viewer); // true sólo cuando se confirmó una escena de la revisión vigente.
    void cancel();
    void clearCache();
    StreamInfo info() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace enc
