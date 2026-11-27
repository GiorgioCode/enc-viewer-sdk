// drawing.cpp — Primitivas de la interfaz del ejemplo, ajenas al renderizador ENC.
// La fuente de mapa de bits y los colores pertenecen al consumidor y no utilizan cabeceras privadas.
#include "drawing.h"
#include <GL/gl.h>
#include <array>
#include <cctype>
#include <unordered_map>
namespace demo {
namespace {
// Consulta la fuente de mapa de bits de 5 por 7; los caracteres desconocidos quedan vacíos.
const std::array<unsigned char, 7>& glyph(char c) {
    static const std::array<unsigned char, 7> blank{};
    static const std::unordered_map<char, std::array<unsigned char, 7>> font = {
        {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
        {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
        {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
        {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
        {'I', {14, 4, 4, 4, 4, 4, 14}},      {'J', {7, 2, 2, 2, 18, 18, 12}},
        {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
        {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
        {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
        {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
        {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
        {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
        {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
        {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
        {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
        {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
        {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
        {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
        {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
        {'-', {0, 0, 0, 31, 0, 0, 0}},       {'_', {0, 0, 0, 0, 0, 0, 31}},
        {'.', {0, 0, 0, 0, 0, 12, 12}},      {',', {0, 0, 0, 0, 4, 4, 8}},
        {':', {0, 12, 12, 0, 12, 12, 0}},    {'/', {1, 2, 2, 4, 8, 8, 16}},
        {'\\', {16, 8, 8, 4, 2, 2, 1}},      {'(', {2, 4, 8, 8, 8, 4, 2}},
        {')', {8, 4, 2, 2, 2, 4, 8}},        {'<', {2, 4, 8, 16, 8, 4, 2}},
        {'>', {8, 4, 2, 1, 2, 4, 8}},        {'=', {0, 31, 0, 31, 0, 0, 0}},
        {'+', {0, 4, 4, 31, 4, 4, 0}},       {'?', {14, 17, 1, 2, 4, 0, 4}},
        {'%', {17, 18, 2, 4, 8, 9, 17}},     {'[', {14, 8, 8, 8, 8, 8, 14}},
        {']', {14, 2, 2, 2, 2, 2, 14}},      {'!', {4, 4, 4, 4, 4, 0, 4}},
        {' ', {0, 0, 0, 0, 0, 0, 0}}};
    auto found = font.find(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    return found == font.end() ? blank : found->second;
}
} // namespace

void colour(unsigned value) {
    glColor3ub(static_cast<GLubyte>(value >> 16), static_cast<GLubyte>(value >> 8),
               static_cast<GLubyte>(value));
}
void rectangle(double x, double y, double width, double height, unsigned value) {
    colour(value);
    glBegin(GL_QUADS);
    glVertex2d(x, y);
    glVertex2d(x + width, y);
    glVertex2d(x + width, y + height);
    glVertex2d(x, y + height);
    glEnd();
}
void text(double x, double y, const std::string& value, unsigned color, int maxWidth) {
    colour(color);
    glBegin(GL_QUADS);
    int cursor = 0;
    for (char c : value) {
        if (cursor + 6 > maxWidth)
            break;
        const auto& pattern = glyph(c);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col) {
                if (!(pattern[row] & (1u << (4 - col))))
                    continue;
                const double left = x + cursor + col, top = y + row;
                glVertex2d(left, top);
                glVertex2d(left + 1, top);
                glVertex2d(left + 1, top + 1);
                glVertex2d(left, top + 1);
            }
        cursor += 6;
    }
    glEnd();
}
} // namespace demo
