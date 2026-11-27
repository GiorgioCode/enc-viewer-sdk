// contact_scenario.h — Sensor sintético reutilizable por los ejemplos de implementación.
// Produce informes AIS/radar mediante API pública; no forma parte del paquete de la biblioteca.
#pragma once
#include <enc/enc.h>
namespace enc_examples {
class ContactScenario {
    std::vector<enc::Contact> seeds_;
    double start_ = 0, updated_ = -1;
    enc::Status report(enc::Viewer& viewer, double elapsed);

  public:
    enc::Status initialize(enc::Viewer& viewer, int width, int height);
    enc::Status advance(enc::Viewer& viewer);
};
} // namespace enc_examples
