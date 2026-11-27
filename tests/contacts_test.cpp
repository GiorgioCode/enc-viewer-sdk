// contacts_test.cpp — Gestión de contactos AIS/radar, peligros, pérdida y reconocimiento.
// Verifica unidades CPA/TCPA, filtrado, transacciones y conservación de contactos al cambiar cartas.
#include <cmath>
#include <enc/enc.h>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool b, const char* text) {
    if (!b)
        throw std::runtime_error(text);
}
void ok(const enc::Status& s) {
    if (!s)
        throw std::runtime_error(s.message);
}
void transitions() {
    enc::Viewer v;
    enc::OwnShip own;
    own.position = {0, 0};
    own.courseOverGround = 90;
    own.speedOverGround = 10;
    ok(v.setOwnShip(own));
    enc::Contact target;
    target.id = "crossing";
    target.position = {.1, 0};
    target.courseOverGround = 270;
    target.speedOverGround = 10;
    ok(v.upsertContacts({target}));
    auto settings = v.contactSettings();
    settings.automaticDanger = true;
    settings.tcpaLimitMinutes = 20;
    ok(v.configureContacts(settings));
    ok(v.acknowledgeContact(target.id));
    require(v.contact(target.id)->acknowledged, "No se reconoce peligro inicial.");
    settings.automaticDanger = false;
    ok(v.configureContacts(settings));
    require(!v.contact(target.id)->dangerous && !v.contact(target.id)->acknowledged,
            "Desactivar evaluación conserva reconocimiento sin peligro.");
    settings.automaticDanger = true;
    ok(v.configureContacts(settings));
    require(v.contact(target.id)->dangerous && !v.contact(target.id)->acknowledged,
            "Cambiar configuración silenció una nueva alarma.");
    ok(v.acknowledgeContact(target.id));
    own.courseOverGround = 270;
    ok(v.setOwnShip(own));
    require(!v.contact(target.id)->dangerous, "Cambio de rumbo propio no despeja el encuentro.");
    own.courseOverGround = 90;
    ok(v.setOwnShip(own));
    require(v.contact(target.id)->dangerous && !v.contact(target.id)->acknowledged,
            "Nuevo encuentro por buque propio conserva reconocimiento anterior.");

    ok(v.advanceContacts(180));
    require(v.contact(target.id)->state != enc::ContactState::Lost,
            "Se pierde el contacto antes de superar el límite.");
    ok(v.advanceContacts(.1));
    ok(v.acknowledgeContact(target.id));
    require(!v.contact(target.id)->visible, "No se reconoce la pérdida.");
    ok(v.configureContacts(settings));
    require(!v.contact(target.id)->visible, "Cambiar opciones sin transición rearma la pérdida.");
    settings.lostAfterSeconds = 300;
    ok(v.configureContacts(settings));
    require(v.contact(target.id)->visible && v.contact(target.id)->state != enc::ContactState::Lost,
            "Ampliar límite de edad no recupera la presentación.");
    settings.lostAfterSeconds = 180;
    ok(v.configureContacts(settings));
    require(v.contact(target.id)->visible && !v.contact(target.id)->acknowledged &&
                v.contact(target.id)->state == enc::ContactState::Lost,
            "Nueva pérdida por configuración se oculta sin reconocer.");

    // Una actualización de sensor no reduce el AIS que permanece seleccionado.
    target.timestampSeconds = v.contactTime();
    ok(v.upsertContacts({target}));
    ok(v.selectContact(target.id));
    ok(v.upsertContacts({target}));
    require(v.contact(target.id)->selected &&
                v.contact(target.id)->state == enc::ContactState::Activated,
            "Un informe dormido desactiva el AIS seleccionado.");
}
void proximityAndValidation() {
    enc::Viewer v;
    enc::OwnShip own;
    own.position = {0, 0};
    own.courseOverGround = 90;
    own.speedOverGround = 10;
    ok(v.setOwnShip(own));
    enc::Contact c;
    c.id = "parallel";
    c.position = {.001, 0};
    c.courseOverGround = 90;
    c.speedOverGround = 10;
    ok(v.upsertContacts({c}));
    auto settings = v.contactSettings();
    settings.automaticDanger = true;
    ok(v.configureContacts(settings));
    const auto p = v.contact(c.id);
    require(p->dangerous && p->cpaNm && std::abs(*p->cpaNm - .06) < 1e-9 && p->tcpaMinutes &&
                *p->tcpaMinutes == 0,
            "Proximidad constante no usa CPA actual y TCPA cero.");
    c.source = enc::ContactSource::Associated;
    require(!v.upsertContacts({c}) && v.contact(c.id)->report.source == enc::ContactSource::AIS,
            "Se acepta un asociado dormido o se confirma un informe rechazado.");
    c.state = enc::ContactState::Acquiring;
    require(!v.upsertContacts({c}), "Se acepta asociación con radar aún en adquisición.");
    c.state = enc::ContactState::Activated;
    ok(v.upsertContacts({c}));
    auto future = c;
    future.timestampSeconds = v.contactTime() + 1;
    require(!v.upsertContacts({future}), "Se acepta tiempo futuro respecto del reloj local.");
    c.state = enc::ContactState::Lost;
    ok(v.upsertContacts({c}));

    enc::Contact acquiring;
    acquiring.id = "acquiring";
    acquiring.source = enc::ContactSource::Radar;
    acquiring.state = enc::ContactState::Acquiring;
    acquiring.position = {.2, 0};
    acquiring.automaticAcquisition = true;
    ok(v.upsertContacts({acquiring}));
    ok(v.acknowledgeContact(acquiring.id));
    acquiring.dangerous = true;
    ok(v.upsertContacts({acquiring}));
    require(!v.contact(acquiring.id)->acknowledged,
            "Una adquisición reconocida silencia un peligro posterior.");
    ok(v.acknowledgeContact(acquiring.id));
    require(v.contact(acquiring.id)->acknowledged,
            "No se reconocen simultáneamente adquisición y peligro.");

    // El trabajo de interpolación sigue acotado cuando el paso cae por debajo del ULP del reloj.
    enc::Viewer huge;
    auto hugeSettings = huge.contactSettings();
    hugeSettings.lostAfterSeconds = 1e25;
    ok(huge.configureContacts(hugeSettings));
    ok(huge.advanceContacts(1e20));
    c.id = "large-clock";
    c.state = enc::ContactState::Activated;
    c.timestampSeconds = 1e20 - 32768;
    ok(huge.upsertContacts({c}));
    c.position.lon += .001;
    c.timestampSeconds = 1e20;
    ok(huge.upsertContacts({c}));
    require(huge.contact(c.id)->pastPositions.size() <= huge.contactSettings().historyPoints,
            "Un reloj grande excede el historial configurado.");
}
void historySampling() {
    enc::Viewer v;
    auto settings = v.contactSettings();
    settings.historyPoints = 4;
    settings.lostAfterSeconds = 10000;
    ok(v.configureContacts(settings));
    enc::Contact c;
    c.id = "history";
    c.state = enc::ContactState::Activated;
    c.position = {0, 0};
    ok(v.upsertContacts({c}));
    ok(v.advanceContacts(60));
    c.position.lon = .01;
    ok(v.upsertContacts({c}));
    require(v.contact(c.id)->pastPositions.size() == 2, "Faltan las muestras de 0 y 30 s.");
    ok(v.advanceContacts(3540));
    c.position.lon = .2;
    ok(v.upsertContacts({c}));
    const auto p = v.contact(c.id)->pastPositions;
    require(p.size() == 4 && p.front().lon > .1,
            "El límite de muestreo conserva un punto antiguo entre muestras recientes.");
    const double interval = p[1].lon - p[0].lon;
    for (std::size_t i = 2; i < p.size(); ++i)
        require(std::abs(p[i].lon - p[i - 1].lon - interval) < 1e-12,
                "Las posiciones no están espaciadas por un intervalo temporal uniforme.");
    settings.historyIntervalSeconds = 10;
    ok(v.configureContacts(settings));
    require(v.contact(c.id)->pastPositions.empty(),
            "Cambiar intervalo mezcla muestreos distintos.");
    ok(v.advanceContacts(20));
    c.position.lon = .21;
    ok(v.upsertContacts({c}));
    require(!v.contact(c.id)->pastPositions.empty(),
            "No se reanuda el muestreo con el nuevo paso.");
    settings.lostAfterSeconds = 30;
    ok(v.configureContacts(settings));
    ok(v.advanceContacts(60));
    c.position.lon = .4;
    ok(v.upsertContacts({c}));
    require(v.contact(c.id)->pastPositions.empty(),
            "La reaparición inventa posiciones a través de la pérdida.");
}
} // namespace
int main() {
    try {
        enc::Viewer v;
        enc::Contact ais;
        ais.id = "ais:123";
        ais.name = "Prueba Águila";
        ais.position = {.1, 0};
        ais.heading = 270;
        ais.courseOverGround = 270;
        ais.speedOverGround = 10;
        enc::Contact radar = ais;
        radar.id = "radar:7";
        radar.source = enc::ContactSource::Radar;
        radar.state = enc::ContactState::Activated;
        ok(v.upsertContacts({ais, radar}));
        require(v.contacts().size() == 2, "Los sensores se fusionaron sin autorización.");
        require(!v.upsertContacts({ais, ais}) && v.contacts().size() == 2,
                "Lote duplicado no es transaccional.");
        auto invalid = radar;
        invalid.id = "bad";
        invalid.position.lat = 100;
        require(!v.upsertContacts({ais, invalid}) && v.contacts().size() == 2,
                "Se confirmó un lote inválido.");
        enc::OwnShip own;
        own.position = {0, 0};
        own.courseOverGround = 90;
        own.speedOverGround = 10;
        own.heading = 90;
        ok(v.setOwnShip(own));
        auto settings = v.contactSettings();
        settings.automaticDanger = true;
        settings.tcpaLimitMinutes = 20;
        settings.cpaLimitNm = .5;
        ok(v.configureContacts(settings));
        const auto risk = v.contact(ais.id);
        require(risk && risk->dangerous && risk->state == enc::ContactState::Activated &&
                    risk->cpaNm && *risk->cpaNm < 1e-6 && risk->tcpaMinutes &&
                    std::abs(*risk->tcpaMinutes - 18) < 1e-6,
                "CPA/TCPA o activación por peligro incorrectos.");
        require(!risk->acknowledged && risk->vectorEnd,
                "Peligro nuevo no requiere reconocimiento o carece de vector.");
        ok(v.acknowledgeContact(ais.id));
        require(v.contact(ais.id)->acknowledged, "No se reconoce el peligro.");
        settings.vectors = enc::VectorMode::Relative;
        ok(v.configureContacts(settings));
        require(!v.setOwnShip(std::nullopt), "Vectores relativos sin buque propio aceptados.");
        settings.showSleeping = false;
        settings.automaticDanger = false;
        settings.vectors = enc::VectorMode::True;
        ok(v.configureContacts(settings));
        require(!v.contact(ais.id)->visible && v.contact(radar.id)->visible,
                "Filtro AIS afecta radar o no filtra dormidos.");
        ok(v.selectContact(ais.id));
        require(v.contact(ais.id)->selected &&
                    v.contact(ais.id)->state == enc::ContactState::Activated,
                "Seleccionar no activa AIS.");
        ok(v.advanceContacts(60));
        ais.state = enc::ContactState::Activated;
        ais.position = {.09, 0};
        ok(v.upsertContacts({ais}));
        require(v.contact(ais.id)->pastPositions.size() >= 2,
                "No se conservan posiciones a intervalos de tiempo.");
        auto stale = ais;
        stale.timestampSeconds = 0;
        require(!v.upsertContacts({stale}), "Informe atrasado aceptado.");
        ok(v.advanceContacts(200));
        require(v.contact(ais.id)->state == enc::ContactState::Lost &&
                    !v.contact(ais.id)->vectorEnd,
                "Contacto perdido conserva vectores.");
        ok(v.acknowledgeContact(ais.id));
        require(!v.contact(ais.id)->visible, "Pérdida reconocida sigue presentada.");
        ok(v.upsertContacts({ais}));
        require(v.contact(ais.id)->state == enc::ContactState::Activated &&
                    v.contact(ais.id)->visible,
                "Reaparición no restaura el contacto.");
        v.clearCharts();
        require(v.contacts().size() == 2, "Limpiar cartas eliminó contactos.");
        ok(v.fitBounds({.08, -.01, .11, .01, true}));
        require(!v.pickContacts(v.geoToScreen(ais.position)).empty(),
                "No se selecciona un contacto en pantalla.");
        settings.capacity = 1;
        require(!v.configureContacts(settings), "Reducir capacidad pierde contactos.");
        ok(v.removeContact(radar.id));
        v.clearContacts();
        require(v.contactLayerInfo().count == 0, "Quedaron contactos tras limpiar.");
        const auto across = enc::destination({179.99, 0}, 90, 2);
        require(across.lon < 0 && std::abs(across.lat) < 1e-8, "Vector no cruza el antimeridiano.");
        transitions();
        proximityAndValidation();
        historySampling();
        std::cout << "PASS: contactos múltiples AIS/radar, estados, CPA/TCPA, vectores, historial, "
                     "pérdida y reconocimiento.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
