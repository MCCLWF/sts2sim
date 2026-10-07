#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Entities/Creatures/DamageResult.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct EnvenomPower : PowerModel {
    SIM_MODEL(EnvenomPower, PowerModel)

    void AfterDamageGiven(PlayerChoiceContext& ctx, Creature dealer, const DamageResult& result, ValueProp props,
                          Creature target, CardModel) const {
        if (dealer == Owner() && IsPoweredAttack(props) && result.UnblockedDamage > 0)
            PowerCmd::Apply(PowerId::POISON_POWER, ctx, target, Dec(Amount()), Owner(), CardModel{});
    }
};

}  // namespace sts2
