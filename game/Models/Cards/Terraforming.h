#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Terraforming : CardModel {
    SIM_MODEL(Terraforming, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::VIGOR_POWER, ctx, Owner().Creature(), DynamicVars()["VigorPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
