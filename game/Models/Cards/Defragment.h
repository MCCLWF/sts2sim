// Models/Cards/Defragment.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Defragment : CardModel { SIM_MODEL(Defragment, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, Owner().Creature(),
                        DynamicVars()["FocusPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
