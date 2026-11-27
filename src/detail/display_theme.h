// display_theme.h — Paletas diurna y nocturna de presentación.
// Transforma colores al dibujar y restaura la selección anterior al finalizar cada cuadro.
// Expone los tipos y operaciones que pueden usar otros módulos.

#pragma once

namespace enc::detail::display {
// Restaura la paleta anterior al terminar el cuadro, incluso si cambia la ventana.
class ScopedTheme {
    bool previous;

  public:
    explicit ScopedTheme(bool useNight);
    ~ScopedTheme();
    ScopedTheme(const ScopedTheme&) = delete;
    ScopedTheme& operator=(const ScopedTheme&) = delete;
};

// Traduce colores de presentación sin alterar los valores originales de los datos.
unsigned colour(unsigned day);
// Los contornos de símbolos necesitan contraste distinto del relleno negro.
unsigned symbolColour(unsigned day, bool stroke = false);
} // namespace enc::detail::display
