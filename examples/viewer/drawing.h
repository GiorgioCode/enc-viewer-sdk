// drawing.h — Dibujo mínimo para los controles de la aplicación de ejemplo.
// Opera en coordenadas lógicas de ventana y no forma parte de la biblioteca instalable.
#pragma once
#include <string>
namespace demo {
void colour(unsigned value);
void rectangle(double x, double y, double width, double height, unsigned value);
void text(double x, double y, const std::string& value, unsigned color, int maxWidth = 10000);
} // namespace demo
