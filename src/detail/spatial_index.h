// spatial_index.h — Índice BVH de envolventes para descartar objetos sin recorrer toda la escena.
// Se construye en CPU al confirmar la carga y consulta también copias de longitud ±360 grados.
#pragma once
#include "chart_types.h"
#include <enc/types.h>
namespace enc::detail {
class SpatialIndex {
    struct Node {
        double x1, y1, x2, y2;
        int left = -1, right = -1;
        std::size_t begin, end;
    };
    std::vector<Node> nodes_;
    std::vector<std::size_t> ids_;
    int build(const std::vector<Feature>& features, std::size_t begin, std::size_t end);

  public:
    void rebuild(const std::vector<Feature>& features);
    std::vector<std::size_t> query(const Bounds& region) const;
};
} // namespace enc::detail
