// Models/Relics/PollinousCore.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct PollinousCore : RelicModel {
    SIM_MODEL(PollinousCore, RelicModel)

    int32_t TurnsSeen() const { return extra(0); }
    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return TurnsSeen(); }
    void BeforeHandDraw(Player player, PlayerChoiceContext&, CombatState) const {
        if (player != Owner()) return;
        ++extra(0);
        // TurnsSeen setter -> UpdateDisplay.
        SetStatus(extra(0) == DynamicVars()["Turns"].IntValue() - 1 ? RelicStatus::Active : RelicStatus::Normal);
    }
    void AfterCombatEnd(CombatRoom) const { SetStatus(RelicStatus::Normal); }
    Dec ModifyHandDraw(Player player, Dec count) const {
        return player == Owner() && TurnsSeen() >= DynamicVars()["Turns"].IntValue()
                   ? count + DynamicVars().Cards().BaseValue()
                   : count;
    }
    void AfterModifyingHandDraw() const {
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
