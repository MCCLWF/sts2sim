// Models/Relics/Permafrost.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Combat/CombatManager.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct Permafrost : RelicModel {
    SIM_MODEL(Permafrost, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (room.IsCombatRoom()) extra(0) = 0;
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* cardPlay) const {
        if (extra(0) || !cardPlay || !CombatManager(sim_).IsInProgress()) return;
        if (cardPlay->Card.Owner() != Owner() || cardPlay->Card.Type() != CardType::Power) return;
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars()["Block"], nullptr);
        extra(0) = 1;
    }
};

}  // namespace sts2
