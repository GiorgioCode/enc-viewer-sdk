// contact_scenario.cpp — Escenario de contactos activos, dormidos, adquiridos, perdidos y asociados.
// Separa la generación de informes del renderizador y precarga posiciones pasadas a intervalos regulares.
#include "contact_scenario.h"
#include <array>
namespace enc_examples {
enc::Status ContactScenario::report(enc::Viewer& viewer, double elapsed) {
    auto reports = seeds_;
    for (auto& c : reports) {
        c.timestampSeconds = start_ + elapsed;
        if (c.courseOverGround && c.speedOverGround && c.state != enc::ContactState::Lost)
            c.position = enc::destination(c.position, *c.courseOverGround,
                                          *c.speedOverGround * elapsed / 3600);
    }
    return viewer.upsertContacts(reports);
}
enc::Status ContactScenario::initialize(enc::Viewer& viewer, int width, int height) {
    seeds_.clear();
    start_ = viewer.contactTime();
    updated_ = -1;
    const std::array<std::string, 9> names{
        {"AIS dormido", "Águila activo", "AIS peligro", "AIS perdido", "Radar seguido",
         "Radar adquiriendo", "Radar peligro", "Asociado AIS/Radar", "AIS sin rumbo"}};
    for (std::size_t i = 0; i < names.size(); ++i) {
        enc::Contact c;
        c.id = "demo:" + std::to_string(i + 1);
        c.name = names[i];
        c.position =
            viewer.screenToGeo({width * (.2 + .3 * (i % 3)), height * (.22 + .28 * (i / 3))});
        c.state = enc::ContactState::Activated;
        c.heading = 35 + i * 29;
        c.courseOverGround = c.heading;
        c.speedOverGround = 2. + i * .4;
        if (i == 0)
            c.state = enc::ContactState::Sleeping;
        if (i == 2 || i == 6)
            c.dangerous = true;
        if (i == 3)
            c.state = enc::ContactState::Lost;
        if (i >= 4 && i <= 6)
            c.source = enc::ContactSource::Radar;
        if (i == 5) {
            c.state = enc::ContactState::Acquiring;
            c.automaticAcquisition = true;
        }
        if (i == 7) {
            c.source = enc::ContactSource::Associated;
            c.associatedId = "demo-radar:18";
            c.dimensions = enc::ContactDimensions{80, 20, 12, 12};
        }
        if (i == 8) {
            c.heading.reset();
            c.courseOverGround.reset();
            c.speedOverGround.reset();
        }
        seeds_.push_back(std::move(c));
    }
    enc::OwnShip own;
    own.position = viewer.screenToGeo({width * .5, height * .9});
    own.heading = 0;
    own.courseOverGround = 0;
    own.speedOverGround = 0;
    auto status = viewer.setOwnShip(own);
    if (!status)
        return status;
    // Precarga 90 s de informes manteniendo el reloj monotónico y los intervalos de 30 s.
    status = viewer.advanceContacts(90);
    if (!status)
        return status;
    for (double seconds : {0., 30., 60., 90.}) {
        status = report(viewer, seconds);
        if (!status)
            return status;
    }
    updated_ = viewer.contactTime();
    return {};
}
enc::Status ContactScenario::advance(enc::Viewer& viewer) {
    const double now = viewer.contactTime();
    if (seeds_.empty() || now - updated_ < .5)
        return {};
    updated_ = now;
    return report(viewer, now - start_);
}
} // namespace enc_examples
