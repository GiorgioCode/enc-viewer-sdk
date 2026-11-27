// viewer_impl.h — Estado privado que conecta la API estable con los algoritmos del SDK.
// PIMPL impide que el integrador dependa de GDAL, texturas o estructuras internas de geometría.
#pragma once
#include "detail/app_state.h"
#include <enc/viewer.h>
#include <thread>
namespace enc {
struct Viewer::Impl {
    detail::SceneState scene;
    Status startup;
    std::uint32_t generation = 1;
    OverlayId nextOverlay = 1;
    std::thread::id graphicsThread;
    std::vector<Camera> historyBack, historyForward;
    Impl();
    void commit(detail::SceneState&& candidate, bool fitView);
    detail::RasterLayer* raster(OverlayId id);
    FeatureInfo describe(std::size_t index) const;
};
namespace detail {
bool validGeo(enc::GeoPoint point);
bool finite(double value);
Status graphicsAvailable();
} // namespace detail
} // namespace enc
