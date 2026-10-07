#pragma once

#include "game/Commands/OstyCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BoundPhylactery : RelicModel {
    SIM_MODEL(BoundPhylactery, RelicModel)
    bool SpawnsPets() const { return true; }
    void SummonPet() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        OstyCmd::Summon(ctx, Owner(), DynamicVars().Summon().BaseValue(), *this);
    }
    void BeforeCombatStart() const { SummonPet(); }
    void AfterEnergyResetLate(Player player) const {
        if (player == Owner() && Owner().PlayerCombatState().TurnNumber() != 1) SummonPet();
    }
};

}  // namespace sts2
