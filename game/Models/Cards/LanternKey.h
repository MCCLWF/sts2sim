// Models/Cards/LanternKey.cs
#pragma once

#include "game/Models/CardModel.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct LanternKey : CardModel {
    SIM_MODEL(LanternKey, CardModel)

    int32_t MaxUpgradeLevel() const { return 0; }

    RoomTypeSet ModifyUnknownMapPointRoomTypes(RoomTypeSet roomTypes) const {
        if (Owner().RunState().CurrentActIndex() != 2) return roomTypes;
        RoomTypeSet eventOnly;
        eventOnly.Add(RoomType::Event);
        return eventOnly;
    }

    EventModel ModifyNextEvent(EventModel currentEvent) const {
        if (Owner().RunState().CurrentActIndex() != 2) return currentEvent;
        return EventModel::Canonical(&sim(), EventId::WAR_HISTORIAN_REPY);
    }
};

}  // namespace sts2
