// display_theme.cpp — Paletas diurna y nocturna de presentación.
// Transforma colores al dibujar y restaura la selección anterior al finalizar cada cuadro.
// Contiene la implementación de esta responsabilidad.

#include "display_theme.h"

namespace enc::detail::display {
namespace {
// La selección es local al hilo de renderizado, nunca a los datos de una carta.
thread_local bool night = false;
} // namespace

ScopedTheme::ScopedTheme(bool useNight) : previous(night) {
    night = useNight;
}
ScopedTheme::~ScopedTheme() {
    night = previous;
}

unsigned colour(unsigned day) {
    if (!night)
        return day;
    switch (day) {
    // Paneles, botones, separadores y fondos de etiquetas de la interfaz.
    case 0xF5F4ED:
        return 0x10171E;
    case 0xF3F0E6:
        return 0x151E28;
    case 0xF8F7F1:
        return 0x17222D;
    case 0xEAE8DE:
        return 0x111A23;
    case 0xD5D1C4:
        return 0x33424E;
    case 0x334D53:
        return 0x293D4C;
    case 0xE0E5D8:
    case 0xE1E5DA:
        return 0x283846;
    case 0xD1DAD1:
        return 0x354C59;
    case 0xC9DFCD:
        return 0x304E49;
    case 0xC3C2B9:
        return 0x40505B;
    case 0x466A68:
        return 0x58A692;
    case 0x81999B:
        return 0x090E14;
    case 0xF6F5E9:
        return 0x152430;
    case 0xFFFFFF:
        return 0xCDD4D7;
    case 0xFFFDF8:
        return 0x0E1923;
    // Texto de la interfaz y de las cartas.
    case 0x26373C:
    case 0x263D43:
        return 0xBECCD3;
    case 0x34484D:
    case 0x344B50:
    case 0x344F4E:
        return 0xADBFC9;
    case 0x48635E:
    case 0x46595D:
        return 0x9BAEB8;
    case 0x52656A:
    case 0x526F75:
    case 0x60777A:
        return 0x849CA8;
    case 0x637277:
    case 0x647877:
        return 0x8296A1;
    case 0x718084:
    case 0x71857F:
    case 0x778387:
    case 0x788985:
        return 0x778E9B;
    case 0x405A68:
        return 0x9CB4C4;
    case 0xA13D32:
        return 0xBE746A;
    // Tierra y agua del mundo, superficies ENC y bandas de profundidad.
    case 0xDADCC8:
        return 0x262D31;
    case 0xB7D6DE:
        return 0x0B1C27;
    case 0xACCBD4:
        return 0x20313D;
    case 0x8E9F95:
        return 0x52656D;
    case 0xE2D3A6:
        return 0x33323B;
    case 0x8EC2CE:
        return 0x2B475B;
    case 0xA8CFD5:
        return 0x233C4F;
    case 0xB8D9DB:
        return 0x1A3041;
    case 0xC8E1DF:
        return 0x112633;
    case 0xB9D7D8:
        return 0x193340;
    case 0x304B50:
        return 0x7693A2;
    case 0x725D3D:
    case 0x50493C:
        return 0x898B85;
    case 0x668D98:
    case 0x6B969A:
        return 0x4C7188;
    case 0xB34635:
    case 0x8B3C32:
    case 0xB94735:
        return 0xB76C65;
    case 0x8A5B94:
    case 0x654B77:
        return 0xA675AA;
    case 0xC56F29:
    case 0xD16A30:
    case 0xE9A84C:
        return 0xB58B5D;
    // Las anotaciones del PPI acompañan al fósforo rojo nocturno.
    case 0x25874C:
        return 0xC05D55;
    case 0x279456:
        return 0xB76157;
    case 0x2A9A59:
        return 0xC36B60;
    // Los colores náuticos conservan diferencias reconocibles con menor brillo.
    case 0x25383C:
        return 0x080D13;
    case 0xC43D38:
        return 0xB95850;
    case 0x258B59:
        return 0x46936D;
    case 0xF2CA43:
        return 0xBAA35F;
    case 0xAC438F:
        return 0xA56B9C;
    case 0x447AC2:
        return 0x5078A0;
    case 0x8B9390:
        return 0x657177;
    case 0x926544:
        return 0x7D624F;
    case 0xE6AD38:
        return 0xB18B4E;
    case 0x7856A2:
        return 0x7C6590;
    case 0xE98235:
        return 0xB7794C;
    case 0xE89CAD:
        return 0xAB7C8C;
    case 0x527E91:
    case 0x526970:
    case 0x64777B:
        return 0x7A94A2;
    case 0xE9E3CE:
    case 0xF5F1E5:
        return 0x45434A;
    case 0xBEB8A8:
    case 0xD8D1BC:
    case 0xC5BCAB:
        return 0x4D4A4C;
    default:
        return day;
    }
}

unsigned symbolColour(unsigned day, bool stroke) {
    if (night && stroke && day == 0x25383C)
        return 0x9BAEB9;
    if (night && day == 0xFFFFFF)
        return 0xA6B3BA;
    return colour(day);
}

} // namespace enc::detail::display
