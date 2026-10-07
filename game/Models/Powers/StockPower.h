#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/Monsters/Axebot.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct StockPower : PowerModel {
    SIM_MODEL(StockPower, PowerModel)

    bool ShouldStopCombatFromEnding() const { return true; }

    void AfterDeath(PlayerChoiceContext&, Creature target, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented || target != Owner() || Amount() <= 0) return;
        Axebot respawn(MonsterModel::ToMutable(sim_, MonsterId::AXEBOT));
        respawn.SetStockAmount(Amount() - 1);
        CreatureCmd::Add(respawn, target.CombatState(), target.Side(), target.SlotName());
    }
};

}  // namespace sts2
