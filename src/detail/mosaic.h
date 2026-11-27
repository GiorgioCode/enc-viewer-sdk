// mosaic.h — Selección de celdas por escala y consulta de la cobertura realmente visible.
// Comparte criterios entre renderizado, consultas y carga progresiva para evitar escenas inconsistentes.
#pragma once
#include "app_state.h"
#include <optional>
namespace enc::detail {
double displayScale(const SceneState& scene);
std::vector<std::size_t> activeCells(const SceneState& scene, double margin = 50);
bool cellContains(const CellData& cell, Point point);
bool visibleAt(const SceneState& scene, std::size_t cell, Point point);
bool featureVisible(const SceneState& scene, const Feature& feature, bool selected = false);
bool dangerous(const SceneState& scene, const Feature& feature);
std::optional<double> safetyContour(const SceneState& scene, int cell = -1);
const PortrayalRule* ruleFor(const SceneState& scene, const std::string& objectClass);
} // namespace enc::detail
