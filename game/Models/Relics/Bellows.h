#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Bellows : RelicModel {
    SIM_MODEL(Bellows, RelicModel)

    void AfterPlayerTurnStart(PlayerChoiceContext&, Player player) const {
        if (player != Owner() || !Owner().PlayerCombatState() || Owner().PlayerCombatState().TurnNumber() > 1) return;
        CardCmd::Upgrade(Owner().PlayerCombatState().Hand().Cards());
    }
};

}  // namespace sts2
