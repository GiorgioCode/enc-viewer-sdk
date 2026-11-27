// radar_overlay.cpp — Modelo de simulación de radar PPI, independiente de OpenGL.
// Gestiona alcance, reloj, persistencia, retornos y blancos en coordenadas relativas al origen.
// Contiene la implementación de esta responsabilidad.

#include "radar_overlay.h"

namespace enc::detail::overlays {

// Protege la conversión de longitud cerca de los polos.
double Radar::cosine() const {
    return std::max(.01, std::cos(origin.lat * pi / 180));
}

// Expresa una posición geográfica como desplazamiento este/norte en millas náuticas.
LocalPoint Radar::local(GeoPoint p) const {
    return {(p.lon - origin.lon) * 60 * cosine(), (p.lat - origin.lat) * 60};
}

// Convierte un eco local a WGS84 para que siga la cámara de la carta.
GeoPoint Radar::geographic(LocalPoint p) const {
    return {origin.lon + p.east / (60 * cosine()), origin.lat + p.north / 60};
}

// Obtiene el ángulo del barrido a partir del tiempo simulado y las revoluciones por minuto.
double Radar::sweep() const {
    return std::fmod(seconds * rpm * 2 * pi / 60, 2 * pi);
}

// Avanza sólo cuando el radar está visible y en marcha, limitando los saltos de tiempo.
void Radar::advance(double dt) {
    if (visible && !paused && dt > 0)
        seconds += std::min(dt, .25);
}

// Recorre los alcances normalizados sin salir de los extremos disponibles.
void Radar::changeRange(int direction) {
    auto it = std::min_element(radarRanges.begin(), radarRanges.end(), [&](double a, double b) {
        return std::abs(a - rangeNm) < std::abs(b - rangeNm);
    });
    const auto index = std::clamp(static_cast<int>(it - radarRanges.begin()) + direction, 0,
                                  static_cast<int>(radarRanges.size()) - 1);
    rangeNm = radarRanges[static_cast<std::size_t>(index)];
}

// Atenúa el fósforo según el tiempo transcurrido desde el último paso del barrido.
double Radar::persistence(LocalPoint p) const {
    const double bearing = std::atan2(p.east, p.north);
    // Convierte el retraso angular del barrido en edad del eco, con vuelta modular completa.
    const double ageAngle = std::fmod(sweep() - bearing + 4 * pi, 2 * pi);
    const double revolution = 60 / rpm;
    return .12 + .88 * std::exp(-ageAngle / (2 * pi) * revolution / 1.8);
}

// Descarta los ecos y sus celdas ocupadas antes de reconstruir una escena.
void Radar::clearScene() {
    coast.clear();
    echoBins.clear();
}

// Deduplica retornos próximos y limita el número de muestras a dibujar.
void Radar::addEcho(LocalPoint p, float intensity) {
    if (std::hypot(p.east, p.north) > rangeNm || coast.size() >= 40000)
        return;
    const auto x = static_cast<std::uint32_t>(std::floor((p.east / rangeNm + 1) * 300));
    const auto y = static_cast<std::uint32_t>(std::floor((p.north / rangeNm + 1) * 300));
    // Empaqueta ambas coordenadas de la celda en una clave espacial de 64 bits.
    const auto key = (static_cast<std::uint64_t>(x) << 32) | y;
    if (echoBins.insert(key).second)
        coast.push_back({p, intensity});
}

// Recorta un segmento contra el círculo de alcance y lo muestrea con densidad uniforme.
void Radar::addSegment(GeoPoint start, GeoPoint finish, float intensity) {
    const auto a = local(start), b = local(finish);
    const double dx = b.east - a.east, dy = b.north - a.north;
    const double length2 = dx * dx + dy * dy;
    if (length2 < 1e-18) {
        addEcho(a, intensity);
        return;
    }
    const double linear = 2 * (a.east * dx + a.north * dy);
    const double constant = a.east * a.east + a.north * a.north - rangeNm * rangeNm;
    // Intersección recta/círculo: sólo se muestrea el tramo dentro del alcance del radar.
    const double discriminant = linear * linear - 4 * length2 * constant;
    if (discriminant < 0)
        return;
    const double root = std::sqrt(discriminant);
    const double low = std::max(0.0, (-linear - root) / (2 * length2));
    const double high = std::min(1.0, (-linear + root) / (2 * length2));
    if (low > high)
        return;
    const int samples = std::clamp(
        static_cast<int>(std::ceil(std::sqrt(length2) * (high - low) / (rangeNm / 260))), 1, 1024);
    for (int i = 0; i <= samples; ++i) {
        const double t = low + (high - low) * i / samples;
        addEcho({a.east + t * dx, a.north + t * dy}, intensity);
    }
}

// Desplaza un blanco sintético alrededor de su posición inicial de manera determinista.
LocalPoint Radar::targetAt(std::size_t index, double time) const {
    const auto base = targets[index];
    const double angle = time * .025 + index * 2.1;
    return {base.east + rangeNm * .035 * std::sin(angle),
            base.north + rangeNm * .025 * std::cos(angle)};
}

} // namespace enc::detail::overlays
