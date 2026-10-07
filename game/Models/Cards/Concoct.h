#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Concoct : CardModel {
    SIM_MODEL(Concoct, CardModel)

    sts2::CardMultiplayerConstraint MultiplayerConstraint() const { return sts2::CardMultiplayerConstraint::MultiplayerOnly; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        PowerCmd::Apply(PowerId::CONCOCT_POWER, ctx, play->Target, DynamicVars()["ConcoctPower"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
