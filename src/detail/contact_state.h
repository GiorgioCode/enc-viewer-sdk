// contact_state.h — Estado privado de contactos, reconocimientos y posiciones pasadas.
// No contiene adquisición de sensores ni fusión automática: las asociaciones pertenecen al anfitrión.
#pragma once
#include <enc/contacts.h>
#include <map>
namespace enc::detail {
struct ContactRecord {
    Contact report;
    double timestamp = 0;
    bool acknowledgedDanger = false, acknowledgedLost = false, acknowledgedAcquisition = false;
    struct Position {
        double seconds;
        GeoPoint point;
    };
    std::vector<Position> history;
};
struct ContactsState {
    std::map<ContactId, ContactRecord> records;
    std::optional<ContactId> selected;
    std::optional<OwnShip> ownShip;
    ContactSettings settings;
    double seconds = 0;
};
ContactInfo assessContact(const ContactsState& state, const ContactRecord& record,
                          bool includeHistory = true);
} // namespace enc::detail
