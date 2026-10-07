#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct SignalBoost : CardModel { SIM_MODEL(SignalBoost, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::SIGNAL_BOOST_POWER, ctx, Owner().Creature(), DynamicVars()["SignalBoostPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
