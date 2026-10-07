#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Coolant : CardModel { SIM_MODEL(Coolant, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::COOLANT_POWER, ctx, Owner().Creature(), DynamicVars()["CoolantPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
