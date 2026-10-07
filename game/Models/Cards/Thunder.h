#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Thunder : CardModel { SIM_MODEL(Thunder, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::THUNDER_POWER, ctx, Owner().Creature(), DynamicVars()["ThunderPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
