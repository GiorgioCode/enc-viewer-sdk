// render_primitives.h — Primitivas gráficas y texto compartidos.
// Dibuja rectángulos y una fuente de mapa de bits en coordenadas lógicas, aplicando la paleta vigente.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

#include <string>

namespace enc::detail {

// Aplica la paleta vigente antes de enviar un color RGB y su alfa a OpenGL.
void setHex(unsigned int color, float alpha = 1.0f);

// Dibuja un rectángulo en coordenadas lógicas de pantalla.
void rect(double x, double y, double w, double h, unsigned int color);

// Convierte cada píxel activo de la fuente en un cuadrado y limita el ancho del texto.
void drawText(double x, double y, const std::string& value, unsigned int color = 0x26373C,
              int maxWidth = 10000);

} // namespace enc::detail
