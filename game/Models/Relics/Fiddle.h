// Models/Relics/Fiddle.cs (v0.111.0).
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct Fiddle : RelicModel {
    SIM_MODEL(Fiddle, RelicModel)

    Dec ModifyHandDraw(Player player, Dec count) const {
        return player == Owner() ? count + Dec(DynamicVars().Cards().IntValue()) : count;
    }
    bool ShouldDraw(Player player, bool fromHandDraw) const {
        if (fromHandDraw || player != Owner()) return true;
        return player.Creature().Side() != player.Creature().CombatState().CurrentSide();
    }
    void AfterPreventingDraw() const { Flash(); }
};

}  // namespace sts2
