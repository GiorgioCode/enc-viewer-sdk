// contacts_demo.cpp — Adaptador del escenario sintético al ejemplo con interfaz completa.
// Los mismos informes se reutilizan en enc-showcase para contrastar ambos anfitriones.
#include "app.h"
namespace demo {
void App::createDemoContacts() {
    report(contactScenario.initialize(viewer, mapWidth(), mapHeight()));
}
void App::updateDemoContacts() {
    if (simulateContacts)
        report(contactScenario.advance(viewer));
}
} // namespace demo
