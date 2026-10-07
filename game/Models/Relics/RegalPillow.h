// Models/Relics/RegalPillow.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct RegalPillow : RelicModel {
    SIM_MODEL(RegalPillow, RelicModel)
    Dec ModifyRestSiteHealAmount(Creature creature, Dec amount) const {
        if (creature.Player() != Owner() && creature.PetOwner() != Owner()) return amount;
        return amount + DynamicVars().Heal().BaseValue();
    }
    void AfterRestSiteHeal(Player player, bool) const {
        if (player != Owner()) return;
        Flash();
        SetStatus(RelicStatus::Normal);
    }
    void AfterRoomEntered(AbstractRoom room) const {
        SetStatus(room.RoomType() == RoomType::RestSite ? RelicStatus::Active : RelicStatus::Normal);
    }
};

}  // namespace sts2
