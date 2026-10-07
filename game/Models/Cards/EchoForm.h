#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct EchoForm : CardModel { SIM_MODEL(EchoForm, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ECHO_FORM_POWER, ctx, Owner().Creature(), DynamicVars()["EchoForm"].BaseValue(), Owner().Creature(), *this);
    }
};
}
