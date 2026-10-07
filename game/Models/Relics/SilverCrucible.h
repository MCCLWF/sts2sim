// Models/Relics/SilverCrucible.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardCmd.h"
#include "game/Rooms/AbstractRoom.h"
namespace sts2 {
struct SilverCrucible : RelicModel {
    SIM_MODEL(SilverCrucible, RelicModel)
    bool IsAllowed(RunState runState) const { return runState.Players().size() == 1; }
    bool IsUsedUp() const { return extra(0) >= 3 && extra(1) > 0; }
    bool ShowCounter() const { return extra(0) < 3; }
    int32_t DisplayAmount() const { return 3 - extra(0); }
    bool TryModifyCardRewardOptionsLate(Player player, CardList& cards, const CardCreationOptionsState& options) const {
        if (player != Owner() || extra(0) >= 3 || !has_flag(options.flags, CardCreationFlags::IsCardReward)) return false;
        for (CardModel c : cards) if (c.IsUpgradable()) { CardCmd::Upgrade(c); }
        return true;
    }
    // The TimesUsed setter calls CheckIfUsedUp, as the TreasureRoomsEntered setter does.
    void AfterModifyingCardRewardOptions() const {
        if (extra(0) >= 3) return;
        ++extra(0);
        if (IsUsedUp()) SetStatus(RelicStatus::Disabled);
    }
    void AfterRoomEntered(AbstractRoom room) const { if (room.RoomType() == RoomType::Treasure) ++extra(1); if (IsUsedUp()) SetStatus(RelicStatus::Disabled); }
    bool ShouldGenerateTreasure(Player player) const { return player != Owner() || extra(1) > 1; }
};
}
