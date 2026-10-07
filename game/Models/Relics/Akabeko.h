#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Akabeko : RelicModel {
    SIM_MODEL(Akabeko, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature()) || !Owner().PlayerCombatState() ||
            Owner().PlayerCombatState().TurnNumber() > 1) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VIGOR_POWER, ctx, Owner().Creature(),
                        DynamicVars()["VigorPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
