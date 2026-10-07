#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Loop : CardModel { SIM_MODEL(Loop, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::LOOP_POWER, ctx, Owner().Creature(), DynamicVars()["Loop"].BaseValue(), Owner().Creature(), *this);
    }
};
}
