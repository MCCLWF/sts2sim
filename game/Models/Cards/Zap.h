// Models/Cards/Zap.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Zap : CardModel {
    SIM_MODEL(Zap, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { OrbCmd::Channel(OrbId::LIGHTNING_ORB, ctx, Owner()); }
};
}
