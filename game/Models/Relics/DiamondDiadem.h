// Models/Relics/DiamondDiadem.cs (v0.111.0).
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct DiamondDiadem : RelicModel {
    SIM_MODEL(DiamondDiadem, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature()) || Owner().PlayerCombatState().TurnNumber() > 1) return;
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::BLUR_POWER, context, Owner().Creature(), Dec(1), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
