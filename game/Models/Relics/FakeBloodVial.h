// Models/Relics/FakeBloodVial.cs (v0.111.0).
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct FakeBloodVial : RelicModel {
    SIM_MODEL(FakeBloodVial, RelicModel)
    int32_t MerchantCost() const { return 50; }
    void AfterPlayerTurnStartLate(PlayerChoiceContext&, Player player) const {
        if (player == Owner() && player.PlayerCombatState().TurnNumber() <= 1)
            CreatureCmd::Heal(Owner().Creature(), Dec(DynamicVars().Heal().IntValue()));
    }
};
}
