// streaming.cpp — Trabajador CPU, revisiones cancelables y caché LRU de celdas decodificadas.
// La escena visible sólo se reemplaza al confirmar un resultado vigente; los fallos conservan sus datos.
#include "detail/camera.h"
#include "detail/chart_reader.h"
#include "detail/chart_style.h"
#include "detail/gdal_support.h"
#include "viewer_impl.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <enc/streaming.h>
#include <future>
#include <map>
#include <mutex>
#include <stdexcept>
namespace enc {
namespace {
// La firma incluye tamaño y fecha de cada actualización, no sólo del archivo base.
std::string signature(const CellInfo& cell) {
    std::string out;
    auto add = [&](const std::filesystem::path& p) {
        std::error_code e;
        const auto bytes = std::filesystem::file_size(p, e);
        if (e) {
            out += "missing";
            return;
        }
        const auto time = std::filesystem::last_write_time(p, e);
        out += p.string() + ":" + std::to_string(bytes) + ":" +
               (e ? "error" : std::to_string(time.time_since_epoch().count())) + ";";
    };
    add(cell.path);
    std::vector<std::filesystem::path> updates;
    std::error_code error;
    for (std::filesystem::directory_iterator i(cell.path.parent_path(), error), end;
         !error && i != end; i.increment(error)) {
        const auto p = i->path();
        const auto ext = p.extension().string();
        if (p.stem() == cell.path.stem() && ext.size() == 4 && ext != ".000" &&
            std::all_of(ext.begin() + 1, ext.end(),
                        [](unsigned char c) { return c >= '0' && c <= '9'; }))
            updates.push_back(p);
    }
    std::sort(updates.begin(), updates.end());
    for (const auto& p : updates)
        add(p);
    return out;
}
std::size_t estimate(const detail::SceneState& s) {
    std::size_t bytes = sizeof(s) + s.features.capacity() * sizeof(detail::Feature) +
                        s.layers.capacity() * sizeof(detail::Layer);
    for (const auto& f : s.features) {
        bytes += f.points.capacity() * sizeof(detail::Point) +
                 f.attributes.capacity() * sizeof(detail::Attribute);
        for (const auto& a : f.attributes)
            bytes += a.name.capacity() + a.value.capacity();
        for (const auto& line : f.lines)
            bytes += line.capacity() * sizeof(detail::Point);
        for (const auto& p : f.polygons) {
            bytes += p.triangles.capacity() * sizeof(detail::Point);
            for (const auto& ring : p.rings)
                bytes += ring.capacity() * sizeof(detail::Point);
        }
    }
    for (const auto& cell : s.cells)
        for (const auto& p : cell.coverage) {
            bytes += p.triangles.capacity() * sizeof(detail::Point);
            for (const auto& ring : p.rings)
                bytes += ring.capacity() * sizeof(detail::Point);
        }
    return bytes;
}
void append(detail::SceneState& out, const detail::SceneState& cell) {
    const auto layerOffset = out.layers.size(), cellOffset = out.cells.size();
    out.layers.insert(out.layers.end(), cell.layers.begin(), cell.layers.end());
    out.cells.insert(out.cells.end(), cell.cells.begin(), cell.cells.end());
    for (auto f : cell.features) {
        f.layer += layerOffset;
        f.cell += cellOffset;
        out.features.push_back(std::move(f));
    }
    out.indexedCells.insert(out.indexedCells.end(), cell.indexedCells.begin(),
                            cell.indexedCells.end());
    if (!out.hasBounds) {
        out.minLon = cell.minLon;
        out.maxLon = cell.maxLon;
        out.minLat = cell.minLat;
        out.maxLat = cell.maxLat;
    } else {
        out.minLon = std::min(out.minLon, cell.minLon);
        out.maxLon = std::max(out.maxLon, cell.maxLon);
        out.minLat = std::min(out.minLat, cell.minLat);
        out.maxLat = std::max(out.maxLat, cell.maxLat);
    }
    out.hasBounds |= cell.hasBounds;
}
} // namespace
struct ChartStreamer::Impl {
    Catalog catalog;
    StreamOptions options;
    struct Entry {
        std::shared_ptr<const detail::SceneState> scene;
        std::string signature;
        std::size_t bytes;
        std::uint64_t use;
    };
    struct Job {
        std::uint64_t revision;
        std::vector<CellInfo> cells;
        Bounds region;
    };
    struct Result {
        std::uint64_t revision;
        detail::SceneState scene;
        Status status;
    };
    std::map<std::filesystem::path, Entry> cache;
    std::mutex mutex;
    std::size_t cacheBytes = 0, hits = 0, misses = 0;
    std::uint64_t clock = 0;
    std::atomic<std::uint64_t> revision{0};
    std::atomic<std::size_t> completed{0}, total{0};
    std::future<Result> future;
    std::optional<Job> pending;
    Status last;
    Impl(const Catalog& c, const StreamOptions& o) : catalog(c), options(o) {}
    void evict() {
        while (cacheBytes > options.cacheBudgetBytes && !cache.empty()) {
            auto victim =
                std::min_element(cache.begin(), cache.end(), [](const auto& a, const auto& b) {
                    return a.second.use < b.second.use;
                });
            cacheBytes -= victim->second.bytes;
            cache.erase(victim);
        }
    }
    Result run(Job job) {
        Result out;
        out.revision = job.revision;
        std::size_t readable = 0;
        try {
            for (const auto& cell : job.cells) {
                if (revision != job.revision) {
                    out.status = {ErrorCode::Cancelled, "Solicitud reemplazada o cancelada.", {}};
                    return out;
                }
                const auto stamp = signature(cell);
                std::shared_ptr<const detail::SceneState> decoded;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    auto found = cache.find(cell.path);
                    if (found != cache.end() && found->second.signature == stamp) {
                        decoded = found->second.scene;
                        found->second.use = ++clock;
                        ++hits;
                    }
                }
                if (!decoded) {
                    auto single = std::make_shared<detail::SceneState>();
                    detail::GdalMessages messages(cell.path);
                    const bool read = detail::addCell(*single, cell.path) && single->hasBounds;
                    out.status.diagnostics.insert(out.status.diagnostics.end(),
                                                  messages.messages.begin(),
                                                  messages.messages.end());
                    const bool error =
                        std::any_of(messages.messages.begin(), messages.messages.end(),
                                    [](const auto& d) { return d.severity == Severity::Error; });
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        ++misses;
                    }
                    if (!read || error || signature(cell) != stamp) {
                        out.status.diagnostics.push_back(
                            {Severity::Error, cell.path,
                             "Celda ilegible o modificada durante la lectura."});
                        if (!options.allowPartial) {
                            out.status.code = ErrorCode::Decode;
                            out.status.message = "Falló la lectura progresiva.";
                            return out;
                        }
                        ++completed;
                        continue;
                    }
                    decoded = single;
                    const auto bytes = estimate(*single);
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        auto found = cache.find(cell.path);
                        if (found != cache.end()) {
                            cacheBytes -= found->second.bytes;
                            cache.erase(found);
                        }
                        if (bytes <= options.cacheBudgetBytes) {
                            cache.emplace(cell.path, Entry{decoded, stamp, bytes, ++clock});
                            cacheBytes += bytes;
                            evict();
                        }
                    }
                }
                ++readable;
                // Un catálogo sin inspección incluye candidatos de posición desconocida.
                // La lectura los puede cachear, pero sólo incorpora los que intersectan la vista.
                if (std::any_of(decoded->cells.begin(), decoded->cells.end(), [&](const auto& c) {
                        return !c.bounds.valid || c.bounds.intersects(job.region);
                    }))
                    append(out.scene, *decoded);
                ++completed;
            }
            if (!job.cells.empty() && readable == 0) {
                out.status = {ErrorCode::Decode, "Ninguna celda pudo incorporarse.",
                              out.status.diagnostics};
                return out;
            }
            detail::prepareSymbols(out.scene);
            out.scene.spatial.rebuild(out.scene.features);
        } catch (const std::exception& e) {
            out.status = {ErrorCode::Decode, e.what(), {}};
        }
        return out;
    }
    void launch(Job job) {
        completed = 0;
        total = job.cells.size();
        future = std::async(std::launch::async,
                            [this, job = std::move(job)]() mutable { return run(std::move(job)); });
    }
};
ChartStreamer::ChartStreamer(const Catalog& catalog, const StreamOptions& options)
    : impl_(std::make_unique<Impl>(catalog, options)) {
    if (!options.cacheBudgetBytes || !options.maxCells)
        throw std::invalid_argument("La caché y maxCells deben ser mayores que cero.");
}
ChartStreamer::~ChartStreamer() {
    cancel();
    if (impl_->future.valid())
        impl_->future.wait();
}
Status ChartStreamer::requestView(const Bounds& region, double scale) {
    if (!region.valid || !detail::validGeo({region.west, region.south}) ||
        !detail::validGeo({region.east, region.north}) || region.south > region.north ||
        !std::isfinite(scale) || scale < 0)
        return detail::invalid("Región o escala de solicitud inválida.");
    auto& p = *impl_;
    std::vector<CellInfo> cells;
    for (const auto& cell : p.catalog.cells())
        if (!cell.bounds.valid || cell.bounds.intersects(region))
            cells.push_back(cell);
    std::stable_sort(cells.begin(), cells.end(), [&](const auto& a, const auto& b) {
        // Prefiere las escalas adecuadas sin descartar cartas generales de regiones vecinas.
        const double da = scale > 0 && a.compilationScale > 0
                              ? std::abs(std::log(scale / a.compilationScale))
                              : 0;
        const double db = scale > 0 && b.compilationScale > 0
                              ? std::abs(std::log(scale / b.compilationScale))
                              : 0;
        return da == db ? a.path < b.path : da < db;
    });
    if (cells.size() > p.options.maxCells)
        return detail::invalid("La región excede maxCells; reducir la vista o ampliar el límite.");
    Impl::Job job{++p.revision, std::move(cells), region};
    p.last = {};
    if (p.future.valid())
        p.pending = std::move(job);
    else
        p.launch(std::move(job));
    return {};
}
bool ChartStreamer::poll(Viewer& viewer) {
    auto& p = *impl_;
    bool committed = false;
    if (p.future.valid() &&
        p.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto result = p.future.get();
        if (result.revision == p.revision) {
            p.last = std::move(result.status);
            if (p.last) {
                viewer.impl_->commit(std::move(result.scene), false);
                committed = true;
            }
        }
        if (p.pending) {
            auto job = std::move(*p.pending);
            p.pending.reset();
            p.launch(std::move(job));
        }
    }
    return committed;
}
void ChartStreamer::cancel() {
    auto& p = *impl_;
    ++p.revision;
    p.pending.reset();
    p.last = {ErrorCode::Cancelled, "Carga progresiva cancelada.", {}};
}
void ChartStreamer::clearCache() {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.mutex);
    p.cache.clear();
    p.cacheBytes = 0;
}
StreamInfo ChartStreamer::info() const {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.mutex);
    return {p.future.valid() || p.pending.has_value(),
            p.revision,
            p.completed,
            p.total,
            p.cacheBytes,
            p.cache.size(),
            p.hits,
            p.misses,
            p.last};
}
} // namespace enc
