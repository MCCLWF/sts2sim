#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct MachineLearning : CardModel { SIM_MODEL(MachineLearning, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::MACHINE_LEARNING_POWER, ctx, Owner().Creature(), DynamicVars().Cards().BaseValue(), Owner().Creature(), *this);
    }
};
}
