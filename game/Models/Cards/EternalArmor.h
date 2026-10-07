// Models/Cards/EternalArmor.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct EternalArmor : CardModel {
    SIM_MODEL(EternalArmor, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        (void)play;
        PowerCmd::Apply(PowerId::PLATING_POWER, ctx, Owner().Creature(), DynamicVars()["PlatingPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
