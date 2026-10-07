#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ShadowmeldPower : PowerModel {
    SIM_MODEL(ShadowmeldPower, PowerModel)

    Dec ModifyBlockMultiplicative(Creature target, Dec, ValueProp, CardModel, const CardPlay*) const {
        if (target != Owner()) return Dec(1);
        Dec multiplier(1);
        for (int32_t i = 0; i < Amount(); ++i) multiplier *= Dec(2);
        return multiplier;
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
