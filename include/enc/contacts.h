// contacts.h — Modelo público de contactos AIS y radar rastreados y su presentación náutica.
// El anfitrión suministra informes WGS84; la biblioteca gestiona representación, selección y evaluación CPA/TCPA.
#pragma once
#include "types.h"
#include <optional>
namespace enc {
using ContactId = std::string;
enum class ContactSource { AIS, Radar, Associated };
enum class ContactState { Sleeping, Activated, Acquiring, Lost };
enum class VectorMode { True, Relative };
struct ContactDimensions {
    double bow = 0, stern = 0, port = 0, starboard = 0;
};
struct Contact {
    ContactId id;
    ContactSource source = ContactSource::AIS;
    ContactState state = ContactState::Sleeping;
    GeoPoint position;
    std::string name, mmsi, associatedId;
    std::optional<double> heading, courseOverGround, speedOverGround, rateOfTurn;
    std::optional<double>
        timestampSeconds; // Reloj monotónico local de contactTime(), nunca UTC implícito.
    std::optional<ContactDimensions> dimensions; // Metros respecto de la posición reportada.
    bool automaticAcquisition = false, testTarget = false;
    bool dangerous = false; // Evaluación externa, combinada con CPA/TCPA si está habilitado.
};
struct OwnShip {
    GeoPoint position;
    std::optional<double> heading, courseOverGround, speedOverGround;
};
struct ContactSettings {
    bool visible = true, showNames = true, showPastPositions = true, showTrueOutline = true;
    bool showSleeping = true, automaticDanger = false, associatedAsAIS = true;
    double vectorMinutes = 6, lostAfterSeconds = 180, historyIntervalSeconds = 30;
    double sleepingRangeNm = 0, cpaLimitNm = .5, tcpaLimitMinutes = 12, symbolScale = 1;
    std::size_t capacity = 1000, historyPoints = 32;
    VectorMode vectors = VectorMode::True;
};
struct ContactInfo {
    Contact report;
    ContactState state = ContactState::Sleeping;
    bool selected = false, dangerous = false, acknowledged = false, visible = true;
    double ageSeconds = 0;
    std::optional<double> rangeNm, bearingDegrees, cpaNm, tcpaMinutes;
    std::optional<GeoPoint> vectorEnd;
    std::vector<GeoPoint> pastPositions;
};
struct ContactLayerInfo {
    std::size_t count = 0, ais = 0, radar = 0, associated = 0, dangerous = 0, lost = 0;
    std::size_t capacity = 0;
    bool nearCapacity = false;
    double seconds = 0;
};
// Posición futura esférica, útil para sensores simulados y vectores; no modifica informes.
GeoPoint destination(GeoPoint origin, double bearingDegrees, double distanceNm);
} // namespace enc
