// Models/Relics/LavaLamp.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Entities/Creatures/DamageResult.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct LavaLamp : RelicModel {
    SIM_MODEL(LavaLamp, RelicModel)

    void AfterRoomEntered(AbstractRoom) const { extra(0) = 0; }

    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result, ValueProp props,
                             Creature, CardModel) const {
        RunState run = Owner().RunState();
        if (run.st().rooms.empty() || !AbstractRoom(sim_, run.st().rooms.back()).IsCombatRoom()) return;
        if (target != Owner().Creature() || result.UnblockedDamage <= 0 || has_flag(props, ValueProp::Unblockable)) return;
        extra(0) = 1;
    }

    bool TryModifyCardRewardOptionsLate(Player player, CardList& cardRewards,
                                        const CardCreationOptionsState&) const {
        RunState run = Owner().RunState();
        if (run.st().rooms.empty() || !AbstractRoom(sim_, run.st().rooms.back()).IsCombatRoom() ||
            player != Owner() || extra(0)) return false;
        for (CardModel& card : cardRewards) {
            if (!card.IsUpgradable()) continue;
            CardModel upgraded = run.CloneCard(card);
            CardCmd::Upgrade(upgraded);
            card = upgraded;
        }
        return true;
    }
};

}  // namespace sts2
