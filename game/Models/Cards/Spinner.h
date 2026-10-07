#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Spinner : CardModel { SIM_MODEL(Spinner, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        if (CurrentUpgradeLevel() > 0) {
            PlayerChoiceContext blocking = PlayerChoiceContext::Throwing();
            OrbCmd::Channel(OrbId::GLASS_ORB, blocking, Owner());
        }
        PowerCmd::Apply(PowerId::SPINNER_POWER, ctx, Owner().Creature(), DynamicVars()["SpinnerPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
