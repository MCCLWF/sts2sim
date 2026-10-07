// Models/Cards/Rainbow.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Rainbow : CardModel { SIM_MODEL(Rainbow, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        OrbCmd::Channel(OrbId::LIGHTNING_ORB, ctx, Owner());
        OrbCmd::Channel(OrbId::FROST_ORB, ctx, Owner());
        OrbCmd::Channel(OrbId::DARK_ORB, ctx, Owner());
    }
};
}
