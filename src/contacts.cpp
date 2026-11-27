// contacts.cpp — Gestión transaccional de múltiples contactos, tiempos y cálculo relativo CPA/TCPA.
// Usa grados verdaderos, nudos y millas náuticas; mantiene posiciones reportadas sin extrapolarlas tácitamente.
#include "detail/camera.h"
#include "detail/contact_state.h"
#include "detail/gdal_support.h"
#include "viewer_impl.h"
#include <algorithm>
#include <cmath>
#include <enc/contacts.h>
#include <set>
namespace enc {
GeoPoint destination(GeoPoint p, double bearing, double distance) {
    if (!detail::validGeo(p) || !std::isfinite(bearing) || !std::isfinite(distance) || distance < 0)
        return p;
    const double angle = distance * 1852 / 6371008.8, lat = p.lat * detail::kPi / 180,
                 lon = p.lon * detail::kPi / 180, b = bearing * detail::kPi / 180;
    const double y = std::asin(std::clamp(
        std::sin(lat) * std::cos(angle) + std::cos(lat) * std::sin(angle) * std::cos(b), -1., 1.));
    const double x = lon + std::atan2(std::sin(b) * std::sin(angle) * std::cos(lat),
                                      std::cos(angle) - std::sin(lat) * std::sin(y));
    return {detail::wrapLongitude(x * 180 / detail::kPi), y * 180 / detail::kPi};
}
namespace {
bool angle(const std::optional<double>& a) {
    return !a || (std::isfinite(*a) && *a >= 0 && *a < 360);
}
bool speed(const std::optional<double>& a) {
    return !a || (std::isfinite(*a) && *a >= 0 && *a <= 200);
}
std::pair<double, double> velocity(const std::optional<double>& course,
                                   const std::optional<double>& speed) {
    if (!course || !speed)
        return {0, 0};
    const double a = *course * detail::kPi / 180;
    return {std::sin(a) * *speed, std::cos(a) * *speed};
}
bool validContact(const Contact& c, double now) {
    if (c.id.empty() || c.id.size() > 128 || !detail::validGeo(c.position) || !angle(c.heading) ||
        !angle(c.courseOverGround) || !speed(c.speedOverGround))
        return false;
    if (c.timestampSeconds && (!std::isfinite(*c.timestampSeconds) || *c.timestampSeconds < 0 ||
                               *c.timestampSeconds > now))
        return false;
    if (c.rateOfTurn && (!std::isfinite(*c.rateOfTurn) || std::abs(*c.rateOfTurn) > 720))
        return false;
    if (c.source != ContactSource::AIS && c.source != ContactSource::Radar &&
        c.source != ContactSource::Associated)
        return false;
    if (c.state != ContactState::Sleeping && c.state != ContactState::Activated &&
        c.state != ContactState::Acquiring && c.state != ContactState::Lost)
        return false;
    if (c.source == ContactSource::Radar && c.state == ContactState::Sleeping)
        return false;
    if (c.source == ContactSource::AIS && c.state == ContactState::Acquiring)
        return false;
    // Una asociación supone AIS activado y radar ya rastreado (OMI, tabla 4).
    if (c.source == ContactSource::Associated &&
        (c.state == ContactState::Sleeping || c.state == ContactState::Acquiring))
        return false;
    if (c.dimensions)
        for (auto d :
             {c.dimensions->bow, c.dimensions->stern, c.dimensions->port, c.dimensions->starboard})
            if (!std::isfinite(d) || d < 0 || d > 1000)
                return false;
    return true;
}
} // namespace
namespace detail {
ContactInfo assessContact(const ContactsState& s, const ContactRecord& r, bool includeHistory) {
    ContactInfo out;
    out.report = r.report;
    out.selected = s.selected && *s.selected == r.report.id;
    out.ageSeconds = std::max(0., s.seconds - r.timestamp);
    out.state = out.ageSeconds > s.settings.lostAfterSeconds ? ContactState::Lost : r.report.state;
    out.dangerous = r.report.dangerous;
    const auto& c = r.report;
    if (s.ownShip) {
        const auto& own = *s.ownShip;
        const double dy = (c.position.lat - own.position.lat) * 60;
        const double dx = wrapLongitude(c.position.lon - own.position.lon) * 60 *
                          std::cos((own.position.lat + c.position.lat) * kPi / 360);
        out.rangeNm = std::hypot(dx, dy);
        out.bearingDegrees = std::fmod(std::atan2(dx, dy) * 180 / kPi + 360, 360.);
        if (own.courseOverGround && own.speedOverGround && c.courseOverGround &&
            c.speedOverGround) {
            const auto target = velocity(c.courseOverGround, c.speedOverGround),
                       ship = velocity(own.courseOverGround, own.speedOverGround);
            const double vx = target.first - ship.first, vy = target.second - ship.second,
                         v2 = vx * vx + vy * vy;
            // Sin movimiento relativo, la distancia actual ya es la mínima:
            // TCPA=0 permite evaluar explícitamente esa proximidad constante.
            const double hours = v2 > 1e-12 ? -(dx * vx + dy * vy) / v2 : 0;
            out.tcpaMinutes = hours * 60;
            out.cpaNm = std::hypot(dx + vx * hours, dy + vy * hours);
            if (s.settings.automaticDanger && out.state != ContactState::Lost && hours >= 0 &&
                *out.tcpaMinutes <= s.settings.tcpaLimitMinutes &&
                *out.cpaNm <= s.settings.cpaLimitNm)
                out.dangerous = true;
        }
    }
    if ((out.dangerous || out.selected) && out.state == ContactState::Sleeping)
        out.state = ContactState::Activated;
    if (c.courseOverGround && c.speedOverGround && out.state != ContactState::Sleeping &&
        out.state != ContactState::Lost) {
        auto v = velocity(c.courseOverGround, c.speedOverGround);
        if (s.settings.vectors == VectorMode::Relative && s.ownShip) {
            const auto own = velocity(s.ownShip->courseOverGround, s.ownShip->speedOverGround);
            v.first -= own.first;
            v.second -= own.second;
        }
        out.vectorEnd = destination(c.position, std::atan2(v.first, v.second) * 180 / kPi,
                                    std::hypot(v.first, v.second) * s.settings.vectorMinutes / 60);
    }
    // Una adquisición reconocida no reconoce también un peligro que aparece después.
    out.acknowledged = out.state == ContactState::Lost ? r.acknowledgedLost
                       : out.state == ContactState::Acquiring
                           ? r.acknowledgedAcquisition && (!out.dangerous || r.acknowledgedDanger)
                           : r.acknowledgedDanger;
    out.visible = s.settings.visible && !(out.state == ContactState::Lost && r.acknowledgedLost);
    if (out.state == ContactState::Sleeping && !out.dangerous && !out.selected)
        out.visible &= s.settings.showSleeping && (!s.settings.sleepingRangeNm || !out.rangeNm ||
                                                   *out.rangeNm <= s.settings.sleepingRangeNm);
    if (includeHistory && s.settings.showPastPositions && out.state != ContactState::Sleeping &&
        out.state != ContactState::Lost)
        for (const auto& p : r.history)
            out.pastPositions.push_back(p.point);
    return out;
}
} // namespace detail
namespace {
struct ContactTransition {
    detail::ContactRecord* record;
    ContactState state;
    bool dangerous;
};
// Los cambios de umbrales y del buque propio también pueden iniciar una alarma nueva.
// Comparar antes/después evita que un reconocimiento anterior silencie su reaparición.
void refreshAcknowledgements(detail::ContactsState& state,
                             const std::vector<ContactTransition>& before) {
    for (const auto& prior : before) {
        auto& record = *prior.record;
        const auto current = detail::assessContact(state, record, false);
        if (!current.dangerous || !prior.dangerous)
            record.acknowledgedDanger = false;
        if (current.state != ContactState::Lost || prior.state != ContactState::Lost)
            record.acknowledgedLost = false;
        if (current.state != ContactState::Acquiring || prior.state != ContactState::Acquiring)
            record.acknowledgedAcquisition = false;
    }
}
std::vector<ContactTransition> contactSnapshot(detail::ContactsState& state) {
    std::vector<ContactTransition> out;
    out.reserve(state.records.size());
    for (auto& item : state.records) {
        const auto info = detail::assessContact(state, item.second, false);
        out.push_back({&item.second, info.state, info.dangerous});
    }
    return out;
}
} // namespace
Status Viewer::configureContacts(const ContactSettings& settings) {
    const auto& s = settings;
    for (auto value : {s.vectorMinutes, s.lostAfterSeconds, s.historyIntervalSeconds,
                       s.sleepingRangeNm, s.cpaLimitNm, s.tcpaLimitMinutes, s.symbolScale})
        if (!std::isfinite(value))
            return detail::invalid("Configuración de contactos no finita.");
    if (s.vectorMinutes < 0 || s.vectorMinutes > 60 || s.lostAfterSeconds <= 0 ||
        s.historyIntervalSeconds <= 0 || s.sleepingRangeNm < 0 || s.cpaLimitNm < 0 ||
        s.tcpaLimitMinutes < 0 || s.symbolScale < .25 || s.symbolScale > 4 || s.capacity == 0 ||
        s.capacity > 10000 || s.capacity < impl_->scene.contacts.records.size() ||
        s.historyPoints > 256 ||
        (s.vectors != VectorMode::True && s.vectors != VectorMode::Relative))
        return detail::invalid(
            "Límites de contactos inválidos o capacidad inferior a la colección actual.");
    if (s.vectors == VectorMode::Relative &&
        (!impl_->scene.contacts.ownShip || !impl_->scene.contacts.ownShip->courseOverGround ||
         !impl_->scene.contacts.ownShip->speedOverGround))
        return detail::invalid("Vectores relativos requieren COG y SOG del buque propio.");
    const auto before = contactSnapshot(impl_->scene.contacts);
    const bool intervalChanged =
        s.historyIntervalSeconds != impl_->scene.contacts.settings.historyIntervalSeconds;
    impl_->scene.contacts.settings = s;
    for (auto& item : impl_->scene.contacts.records) {
        auto& h = item.second.history;
        if (intervalChanged)
            h.clear(); // No combina puntos muestreados con intervalos distintos.
        else if (h.size() > s.historyPoints)
            h.erase(h.begin(), h.end() - s.historyPoints);
    }
    refreshAcknowledgements(impl_->scene.contacts, before);
    return {};
}
ContactSettings Viewer::contactSettings() const {
    return impl_->scene.contacts.settings;
}
Status Viewer::upsertContacts(const std::vector<Contact>& reports) {
    auto& s = impl_->scene.contacts;
    std::set<ContactId> ids;
    std::size_t count = s.records.size();
    // Valida el lote entero antes de confirmar, incluyendo informes atrasados y capacidad.
    for (const auto& report : reports) {
        if (!validContact(report, s.seconds) || !ids.insert(report.id).second)
            return detail::invalid("Contacto inválido o identificador repetido en el lote.");
        const auto it = s.records.find(report.id);
        if (it == s.records.end())
            ++count;
        else if (report.timestampSeconds.value_or(s.seconds) < it->second.timestamp)
            return detail::invalid("Informe de contacto anterior al ya confirmado.");
    }
    if (count > s.settings.capacity)
        return detail::invalid("Capacidad de contactos excedida.");
    for (auto report : reports) {
        const auto it = s.records.find(report.id);
        const bool existed = it != s.records.end();
        auto& record = s.records[report.id];
        const double timestamp = report.timestampSeconds.value_or(s.seconds);
        const auto prior = detail::assessContact(s, record, false);
        const bool brokenTrack =
            prior.state == ContactState::Lost || report.state == ContactState::Lost;
        if (brokenTrack)
            record.history
                .clear(); // La reaparición inicia una traza sin inventar el tramo perdido.
        if (existed && !brokenTrack && timestamp > record.timestamp && s.settings.historyPoints) {
            const double step = s.settings.historyIntervalSeconds;
            const double first =
                std::max(std::ceil(record.timestamp / step) * step,
                         std::ceil(timestamp / step) * step - s.settings.historyPoints * step);
            // El número entero de muestras también acota el trabajo cuando la precisión
            // de un reloj muy grande impide que sumar «step» cambie un double.
            for (std::size_t sample = 0; sample < s.settings.historyPoints; ++sample) {
                const double t = first + sample * step;
                if (!std::isfinite(t) || t >= timestamp)
                    break;
                if (!record.history.empty() && t <= record.history.back().seconds)
                    continue;
                const double fraction = (t - record.timestamp) / (timestamp - record.timestamp);
                record.history.push_back(
                    {t,
                     {detail::wrapLongitude(
                          record.report.position.lon +
                          detail::wrapLongitude(report.position.lon - record.report.position.lon) *
                              fraction),
                      record.report.position.lat +
                          (report.position.lat - record.report.position.lat) * fraction}});
            }
        }
        report.timestampSeconds = timestamp;
        record.report = std::move(report);
        record.timestamp = timestamp;
        if (record.history.size() > s.settings.historyPoints)
            record.history.erase(record.history.begin(),
                                 record.history.end() - s.settings.historyPoints);
        const auto now = detail::assessContact(s, record, false);
        if (!now.dangerous || !prior.dangerous)
            record.acknowledgedDanger = false;
        if (now.state != ContactState::Lost || prior.state != ContactState::Lost)
            record.acknowledgedLost = false;
        if (now.state != ContactState::Acquiring || prior.state != ContactState::Acquiring)
            record.acknowledgedAcquisition = false;
    }
    return {};
}
Status Viewer::removeContact(const ContactId& id) {
    auto& s = impl_->scene.contacts;
    if (!s.records.erase(id))
        return {ErrorCode::NotFound, "Contacto inexistente.", {}};
    if (s.selected && *s.selected == id)
        s.selected.reset();
    return {};
}
void Viewer::clearContacts() {
    impl_->scene.contacts.records.clear();
    impl_->scene.contacts.selected.reset();
}
std::vector<ContactInfo> Viewer::contacts() const {
    std::vector<ContactInfo> out;
    for (const auto& item : impl_->scene.contacts.records)
        out.push_back(detail::assessContact(impl_->scene.contacts, item.second));
    return out;
}
std::optional<ContactInfo> Viewer::contact(const ContactId& id) const {
    const auto& s = impl_->scene.contacts;
    const auto it = s.records.find(id);
    if (it == s.records.end())
        return {};
    return detail::assessContact(s, it->second);
}
Status Viewer::selectContact(std::optional<ContactId> id) {
    auto& s = impl_->scene.contacts;
    if (id && !s.records.count(*id))
        return {ErrorCode::NotFound, "Contacto inexistente.", {}};
    s.selected = std::move(id);
    if (s.selected) {
        auto& c = s.records[*s.selected].report;
        if (c.state == ContactState::Sleeping)
            c.state = ContactState::Activated;
    }
    return {};
}
Status Viewer::acknowledgeContact(const ContactId& id) {
    auto& s = impl_->scene.contacts;
    const auto it = s.records.find(id);
    if (it == s.records.end())
        return {ErrorCode::NotFound, "Contacto inexistente.", {}};
    const auto current = detail::assessContact(s, it->second, false);
    if (current.state == ContactState::Lost)
        it->second.acknowledgedLost = true;
    if (current.dangerous)
        it->second.acknowledgedDanger = true;
    if (current.state == ContactState::Acquiring)
        it->second.acknowledgedAcquisition = true;
    return {};
}
Status Viewer::setOwnShip(std::optional<OwnShip> ship) {
    if (ship && (!detail::validGeo(ship->position) || !angle(ship->heading) ||
                 !angle(ship->courseOverGround) || !speed(ship->speedOverGround)))
        return detail::invalid("Datos de buque propio inválidos.");
    if (impl_->scene.contacts.settings.vectors == VectorMode::Relative &&
        (!ship || !ship->courseOverGround || !ship->speedOverGround))
        return detail::invalid("Conservar COG/SOG propio mientras se usan vectores relativos.");
    const auto before = contactSnapshot(impl_->scene.contacts);
    impl_->scene.contacts.ownShip = ship;
    refreshAcknowledgements(impl_->scene.contacts, before);
    return {};
}
std::optional<OwnShip> Viewer::ownShip() const {
    return impl_->scene.contacts.ownShip;
}
double Viewer::contactTime() const {
    return impl_->scene.contacts.seconds;
}
Status Viewer::advanceContacts(double seconds) {
    auto& s = impl_->scene.contacts;
    if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(s.seconds + seconds))
        return detail::invalid("Tiempo de contactos inválido.");
    const auto before = contactSnapshot(s);
    s.seconds += seconds;
    refreshAcknowledgements(s, before);
    return {};
}
ContactLayerInfo Viewer::contactLayerInfo() const {
    ContactLayerInfo out;
    const auto& s = impl_->scene.contacts;
    out.capacity = s.settings.capacity;
    out.seconds = s.seconds;
    for (const auto& c : contacts()) {
        ++out.count;
        out.ais += c.report.source == ContactSource::AIS;
        out.radar += c.report.source == ContactSource::Radar;
        out.associated += c.report.source == ContactSource::Associated;
        out.dangerous += c.dangerous;
        out.lost += c.state == ContactState::Lost;
    }
    out.nearCapacity = out.count * 10 >= out.capacity * 9;
    return out;
}
std::vector<ContactInfo> Viewer::pickContacts(ScreenPoint point, double tolerance) const {
    std::vector<std::pair<double, ContactInfo>> candidates;
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(tolerance) ||
        tolerance < 0 || tolerance > 100)
        return {};
    for (auto c : contacts())
        if (c.visible) {
            const auto p = geoToScreen(c.report.position);
            const auto d = std::hypot(p.x - point.x, p.y - point.y);
            if (d <= tolerance)
                candidates.emplace_back(d, std::move(c));
        }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<ContactInfo> out;
    for (auto& c : candidates)
        out.push_back(std::move(c.second));
    return out;
}
} // namespace enc
