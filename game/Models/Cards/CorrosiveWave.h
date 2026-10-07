#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct CorrosiveWave : CardModel {
    SIM_MODEL(CorrosiveWave, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::CORROSIVE_WAVE_POWER, ctx, Owner().Creature(), DynamicVars()["CorrosiveWave"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
