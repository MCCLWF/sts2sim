#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Feral : CardModel { SIM_MODEL(Feral, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::FERAL_POWER, ctx, Owner().Creature(), DynamicVars()["FeralPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
