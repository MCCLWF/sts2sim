#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Patter : CardModel {
    SIM_MODEL(Patter, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        PowerCmd::Apply(PowerId::VIGOR_POWER, ctx, Owner().Creature(), DynamicVars()["VigorPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
