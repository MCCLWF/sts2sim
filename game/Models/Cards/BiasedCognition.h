// Models/Cards/BiasedCognition.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct BiasedCognition : CardModel { SIM_MODEL(BiasedCognition, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, Owner().Creature(),
                        DynamicVars()["FocusPower"].BaseValue(), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::BIASED_COGNITION_POWER, ctx, Owner().Creature(),
                        DynamicVars()["BiasedCognitionPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
