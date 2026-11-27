// raster_overlay.h — Modelo y carga de imágenes TIFF y GeoTIFF.
// Normaliza bandas, transparencia y georreferencias; administra ubicación y textura OpenGL diferida.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include "geo_types.h"
#include <GL/gl.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace enc::detail::overlays {
// El anfitrión libera esta colección explícitamente con su contexto vigente.
struct GpuResources {
    std::set<GLuint> live;
    std::vector<GLuint> retired;
    void collect();
    void release();
};
// Destruir el propietario sólo encola; nunca llama OpenGL desde un destructor CPU.
struct Texture {
    GLuint id = 0;
    std::shared_ptr<GpuResources> owner;
    explicit Texture(std::shared_ptr<GpuResources> resources);
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
};
struct Raster {
    int width = 0, height = 0;
    // Píxeles RGBA intercalados en CPU; la textura se crea de forma diferida al dibujar.
    std::vector<std::uint8_t> rgba;
    // Esquinas del borde de píxel en WGS84; sentido horario desde la superior izquierda.
    std::array<GeoPoint, 4> corners{};
    // Las copias de estado comparten la textura; el último propietario libera el recurso GPU.
    std::shared_ptr<Texture> texture;
    std::filesystem::path path;
    bool visible = true, georeferenced = false;
    // Edición de presentación: no modifica los píxeles ni la georreferencia del archivo fuente.
    double opacity = .55, scale = 1, offsetLon = 0, offsetLat = 0;
    // Un archivo sólo se considera cargado cuando existen dimensiones y píxeles válidos.
    bool loaded() const {
        return width > 0 && height > 0 && !rgba.empty();
    }
    // Centro de las esquinas originales y posición final tras la edición manual.
    GeoPoint centre() const;
    GeoPoint placedCorner(std::size_t i) const;
    // Restaura la huella fuente sin recargar el TIFF.
    void resetPlacement() {
        scale = 1;
        offsetLon = offsetLat = 0;
    }
    // Escala la imagen manteniendo fijo el anclaje geográfico del mouse.
    void scaleAt(GeoPoint anchor, double factor);
    // Crea y rellena la textura una sola vez dentro de un contexto OpenGL vigente.
    void upload(const std::shared_ptr<GpuResources>& resources);
};

// La carga es transaccional: un error conserva la imagen anterior. El trabajo de GPU
// se difiere hasta upload(), permitiendo cargar y reproyectar sin contexto OpenGL.
bool loadRaster(Raster& result, const std::filesystem::path& path,
                const std::array<GeoPoint, 4>& fallbackExtent, std::string& error);
// Escribe un GeoTIFF sintético transparente para probar las capas sin datos externos.
bool createDemoTiff(const std::filesystem::path& path, const std::array<GeoPoint, 4>& extent,
                    std::string& error);
} // namespace enc::detail::overlays
