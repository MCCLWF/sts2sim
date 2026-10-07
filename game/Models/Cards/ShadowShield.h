#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct ShadowShield : CardModel { SIM_MODEL(ShadowShield, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        OrbCmd::Channel(OrbId::DARK_ORB, ctx, Owner());
    }
};
}
