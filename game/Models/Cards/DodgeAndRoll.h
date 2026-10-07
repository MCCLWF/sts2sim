// Models/Cards/DodgeAndRoll.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct DodgeAndRoll : CardModel {
    SIM_MODEL(DodgeAndRoll, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        Dec amount = CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        PowerCmd::Apply(PowerId::BLOCK_NEXT_TURN_POWER, ctx, Owner().Creature(), amount,
                        Owner().Creature(), *this);
    }
};
}
