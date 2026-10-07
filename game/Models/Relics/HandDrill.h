// Models/Relics/HandDrill.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct HandDrill : RelicModel {
    SIM_MODEL(HandDrill, RelicModel)

    void AfterBlockBroken(PlayerChoiceContext& ctx, Creature target, Creature breaker) const {
        if ((breaker == Owner().Creature() || (breaker && breaker.PetOwner() == Owner())) && !target.IsPlayer())
            PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, target, DynamicVars().Vulnerable().BaseValue(),
                            Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
