#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/ForgeCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct BigBang : CardModel {
    SIM_MODEL(BigBang, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
        ForgeCmd::Forge(DynamicVars().Forge().IntValue(), Owner(), *this);
    }
};
}  // namespace sts2
