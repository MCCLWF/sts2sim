// Models/Relics/FakeVenerableTeaSet.cs (v0.111.0).
#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
namespace sts2 {
struct FakeVenerableTeaSet : RelicModel {
    SIM_MODEL(FakeVenerableTeaSet, RelicModel)
    int32_t MerchantCost() const { return 50; }
    void AfterRoomEntered(AbstractRoom room) const {
        if (room.RoomType() != RoomType::RestSite) return;
        extra(0) = 1;
        SetStatus(RelicStatus::Active);
    }
    void AfterEnergyReset(Player player) const {
        if (player != Owner() || extra(0) == 0) return;
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }
};
}
