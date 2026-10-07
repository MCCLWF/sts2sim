#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct ConsumingShadow : CardModel { SIM_MODEL(ConsumingShadow, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        for (int32_t i = 0; i < DynamicVars().Repeat().IntValue(); ++i) OrbCmd::Channel(OrbId::DARK_ORB, ctx, Owner());
        PowerCmd::Apply(PowerId::CONSUMING_SHADOW_POWER, ctx, Owner().Creature(), DynamicVars()["ConsumingShadowPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
