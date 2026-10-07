// Models/Relics/Pendulum.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Pendulum : RelicModel {
    SIM_MODEL(Pendulum, RelicModel)

    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return extra(0); }

    void BeforeHandDraw(Player player, PlayerChoiceContext&, CombatState) const {
        if (player != Owner()) return;
        int32_t turns = DynamicVars()["Turns"].IntValue();
        extra(0) = (extra(0) + 1) % turns;
        SetStatus(extra(0) == turns - 1 ? RelicStatus::Active : RelicStatus::Normal);
    }

    Dec ModifyHandDraw(Player player, Dec count) const {
        if (player != Owner() || extra(0) != 0) return count;
        return count + DynamicVars()["Cards"].BaseValue();
    }

    void AfterCombatEnd(CombatRoom) const {
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
