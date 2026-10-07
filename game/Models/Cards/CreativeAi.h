#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct CreativeAi : CardModel { SIM_MODEL(CreativeAi, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::CREATIVE_AI_POWER, ctx, Owner().Creature(), DynamicVars()["CreativeAi"].BaseValue(), Owner().Creature(), *this);
    }
};
}
