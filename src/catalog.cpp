// catalog.cpp — Descubrimiento e inspección transaccional de colecciones ENC.
// Mantiene rutas canónicas, metadatos DSID, cobertura y actualizaciones; no crea contexto gráfico.
#include "detail/gdal_support.h"
#include <algorithm>
#include <cctype>
#include <cpl_json.h>
#include <enc/catalog.h>
#include <ogrsf_frmts.h>
#include <set>

namespace enc {
namespace {
namespace fs = std::filesystem;
std::string extension(const fs::path& path) {
    auto value = path.extension().string();
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
int updateIndex(const fs::path& path) {
    const auto text = extension(path);
    if (text.size() != 4 ||
        !std::all_of(text.begin() + 1, text.end(), [](unsigned char c) { return std::isdigit(c); }))
        return -1;
    return std::stoi(text.substr(1));
}
// Sólo se consideran actualizaciones con el mismo nombre base dentro de la carpeta de la celda.
void inspectUpdates(CellInfo& cell, std::vector<Diagnostic>& diagnostics) {
    std::error_code error;
    for (fs::directory_iterator it(cell.path.parent_path(), error), end; !error && it != end;
         it.increment(error)) {
        if (it->path().stem() == cell.path.stem() && updateIndex(it->path()) > 0 &&
            it->is_regular_file(error))
            cell.updates.push_back(it->path());
    }
    std::sort(cell.updates.begin(), cell.updates.end(),
              [](const auto& a, const auto& b) { return updateIndex(a) < updateIndex(b); });
    int expected = 1;
    for (const auto& update : cell.updates) {
        if (updateIndex(update) != expected)
            diagnostics.push_back(
                {Severity::Warning, update,
                 "Secuencia de actualizaciones incompleta; revisar diagnóstico GDAL."});
        expected = updateIndex(update) + 1;
        const auto time = fs::last_write_time(update, error);
        if (!error)
            cell.modifiedTicks = std::max(
                cell.modifiedTicks, static_cast<std::int64_t>(time.time_since_epoch().count()));
    }
}
// DSID aporta edición, actualización y escala; M_COVR aporta una envolvente para consultas espaciales.
void inspectMetadata(CellInfo& cell, std::vector<Diagnostic>& diagnostics) {
    detail::GdalMessages capture(cell.path);
    auto dataset = detail::openCell(cell.path);
    cell.readable = static_cast<bool>(dataset);
    if (dataset) {
        if (auto* layer = dataset->GetLayerByName("DSID")) {
            std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(
                layer->GetNextFeature(), OGRFeature::DestroyFeature);
            if (feature) {
                const auto text = [&](const char* name) {
                    const int i = feature->GetFieldIndex(name);
                    return i >= 0 ? std::string(feature->GetFieldAsString(i)) : std::string{};
                };
                const auto number = [&](const char* name) {
                    const int i = feature->GetFieldIndex(name);
                    return i >= 0 ? feature->GetFieldAsInteger(i) : 0;
                };
                cell.edition = text("DSID_EDTN");
                cell.updateNumber = text("DSID_UPDN");
                cell.issueDate = text("DSID_ISDT");
                cell.compilationScale = number("DSPM_CSCL");
                cell.intendedUsage = number("DSID_INTU");
            }
        }
        if (auto* coverage = dataset->GetLayerByName("M_COVR")) {
            OGREnvelope bounds;
            if (coverage->GetExtent(&bounds, TRUE) == OGRERR_NONE)
                cell.bounds = {bounds.MinX, bounds.MinY, bounds.MaxX, bounds.MaxY, true};
        }
    } else
        diagnostics.push_back({Severity::Error, cell.path, "No se pudo leer la celda S-57."});
    diagnostics.insert(diagnostics.end(), capture.messages.begin(), capture.messages.end());
}
} // namespace

Status Catalog::scan(const fs::path& source, const ScanOptions& options) {
    if (auto initialized = initialize(); !initialized)
        return initialized;
    std::error_code error;
    const auto canonical = fs::weakly_canonical(source, error);
    if (source.empty() || error || !fs::exists(canonical, error))
        return {ErrorCode::Io, "No existe la carpeta o celda solicitada: " + source.string(), {}};
    std::set<fs::path> paths;
    Status status;
    // No se siguen enlaces simbólicos de directorios: evita ciclos durante el recorrido recursivo.
    const auto consider = [&](const fs::directory_entry& entry) {
        std::error_code local;
        if (extension(entry.path()) == ".000" && entry.is_regular_file(local)) {
            const auto path = fs::weakly_canonical(entry.path(), local);
            if (!local)
                paths.insert(path);
        }
    };
    if (fs::is_regular_file(canonical, error)) {
        if (extension(canonical) != ".000")
            return detail::invalid("Una celda base debe tener extensión .000.");
        paths.insert(canonical);
    } else if (fs::is_directory(canonical, error)) {
        if (options.recursive) {
            for (fs::recursive_directory_iterator
                     it(canonical, fs::directory_options::skip_permission_denied, error),
                 end;
                 !error && it != end; it.increment(error))
                consider(*it);
        } else {
            for (fs::directory_iterator
                     it(canonical, fs::directory_options::skip_permission_denied, error),
                 end;
                 !error && it != end; it.increment(error))
                consider(*it);
        }
        if (error)
            return {ErrorCode::Io, "No se pudo completar el recorrido: " + error.message(), {}};
    } else
        return detail::invalid("La ruta debe ser una carpeta o una celda base.");

    std::vector<CellInfo> candidate;
    for (const auto& path : paths) {
        if (options.progress && !options.progress({candidate.size(), paths.size(), path}))
            return {
                ErrorCode::Cancelled, "Indexación cancelada; se conserva el índice anterior.", {}};
        CellInfo cell;
        cell.path = path;
        cell.name = path.stem().string();
        cell.bytes = fs::file_size(path, error);
        if (error)
            return {ErrorCode::Io, "No se pudo consultar el tamaño de " + path.string(), {}};
        const auto time = fs::last_write_time(path, error);
        if (!error)
            cell.modifiedTicks = static_cast<std::int64_t>(time.time_since_epoch().count());
        inspectUpdates(cell, status.diagnostics);
        if (options.inspectMetadata)
            inspectMetadata(cell, status.diagnostics);
        candidate.push_back(std::move(cell));
    }
    if (options.progress && !options.progress({candidate.size(), paths.size(), {}}))
        return {ErrorCode::Cancelled, "Indexación cancelada; se conserva el índice anterior.", {}};
    source_ = canonical;
    cells_ = std::move(candidate);
    status.message = std::to_string(cells_.size()) + " celdas indexadas.";
    return status;
}
Status Catalog::refresh(const ScanOptions& options) {
    if (source_.empty())
        return detail::invalid("No hay una fuente anterior para actualizar.");
    return scan(source_, options);
}
const std::vector<CellInfo>& Catalog::cells() const noexcept {
    return cells_;
}
const fs::path& Catalog::source() const noexcept {
    return source_;
}
std::vector<CellInfo> Catalog::query(const Bounds& bounds) const {
    std::vector<CellInfo> result;
    for (const auto& cell : cells_)
        if (cell.bounds.intersects(bounds))
            result.push_back(cell);
    return result;
}
Status Catalog::saveInventory(const fs::path& destination) const {
    CPLJSONObject root;
    root.Add("format", "enc-sdk-inventory-1");
    root.Add("source", source_.string());
    CPLJSONArray cells;
    for (const auto& cell : cells_) {
        CPLJSONObject item;
        item.Add("path", cell.path.string());
        item.Add("name", cell.name);
        item.Add("edition", cell.edition);
        item.Add("update", cell.updateNumber);
        item.Add("scale", cell.compilationScale);
        item.Add("readable", cell.readable);
        item.Add("bytes", static_cast<GIntBig>(cell.bytes));
        item.Add("modified_ticks", static_cast<GIntBig>(cell.modifiedTicks));
        CPLJSONArray updates;
        for (const auto& path : cell.updates)
            updates.Add(path.string());
        item.Add("updates", updates);
        CPLJSONArray bounds;
        if (cell.bounds.valid) {
            bounds.Add(cell.bounds.west);
            bounds.Add(cell.bounds.south);
            bounds.Add(cell.bounds.east);
            bounds.Add(cell.bounds.north);
        }
        item.Add("bounds_wgs84", bounds);
        cells.Add(item);
    }
    root.Add("cells", cells);
    CPLJSONDocument document;
    document.SetRoot(root);
    if (!document.Save(destination.string()))
        return {ErrorCode::Io, "No se pudo guardar el inventario JSON.", {}};
    return {};
}
} // namespace enc
