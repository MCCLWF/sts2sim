// Models/Powers/PaperCutsPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Entities/Creatures/DamageResult.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct PaperCutsPower : PowerModel {
    SIM_MODEL(PaperCutsPower, PowerModel)

    void AfterDamageGiven(PlayerChoiceContext& choiceContext, Creature dealer, const DamageResult& result,
                          ValueProp props, Creature target, CardModel) const {
        if (dealer == Owner() && target.IsPlayer() && IsPoweredAttack(props) && result.UnblockedDamage > 0) {
            CreatureCmd::LoseMaxHp(choiceContext, target, Dec(Amount()), false);
        }
    }
};

}  // namespace sts2
