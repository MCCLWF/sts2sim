// Models/Cards/Glacier.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Glacier : CardModel { SIM_MODEL(Glacier, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        OrbCmd::Channel(OrbId::FROST_ORB, ctx, Owner());
        OrbCmd::Channel(OrbId::FROST_ORB, ctx, Owner());
    }
};
}
