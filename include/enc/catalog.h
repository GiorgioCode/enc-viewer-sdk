// catalog.h — Índice reutilizable de celdas S-57 alojadas en una carpeta o archivo individual.
// Descubre bases .000 y actualizaciones vecinas; inspecciona metadatos sin crear recursos gráficos.
#pragma once
#include "types.h"

namespace enc {
struct CellInfo {
    std::filesystem::path path;
    std::string name, edition, updateNumber, issueDate;
    int compilationScale = 0, intendedUsage = 0;
    std::uintmax_t bytes = 0;
    std::int64_t modifiedTicks = 0;
    Bounds bounds;
    bool readable = false;
    std::vector<std::filesystem::path> updates;
};
struct ScanOptions {
    bool recursive = true;
    bool inspectMetadata = true;
    ProgressCallback progress;
};

// Mantiene una instantánea; scan/refresh son explícitos y no instalan vigilantes de archivos.
class Catalog {
  public:
    Status scan(const std::filesystem::path& source, const ScanOptions& options = {});
    Status refresh(const ScanOptions& options = {});
    const std::vector<CellInfo>& cells() const noexcept;
    const std::filesystem::path& source() const noexcept;
    std::vector<CellInfo> query(const Bounds& bounds) const;
    // Exporta un inventario JSON legible para inspección; no es una caché de geometrías.
    Status saveInventory(const std::filesystem::path& destination) const;

  private:
    std::filesystem::path source_;
    std::vector<CellInfo> cells_;
};
} // namespace enc
