#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Ignition : CardModel { SIM_MODEL(Ignition, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target || !play->Target.Player()) throw GameError("Ignition requires a player target");
        OrbCmd::Channel(OrbId::PLASMA_ORB, ctx, play->Target.Player());
    }
};
}
