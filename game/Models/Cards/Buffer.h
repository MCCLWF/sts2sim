#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Buffer : CardModel { SIM_MODEL(Buffer, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::BUFFER_POWER, ctx, Owner().Creature(), DynamicVars()["BufferPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
