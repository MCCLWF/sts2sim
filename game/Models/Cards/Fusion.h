// Models/Cards/Fusion.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Fusion : CardModel { SIM_MODEL(Fusion, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { OrbCmd::Channel(OrbId::PLASMA_ORB, ctx, Owner()); }
};
}
