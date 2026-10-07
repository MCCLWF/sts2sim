#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/ForgeCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct SpoilsOfBattle : CardModel {
    SIM_MODEL(SpoilsOfBattle, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        ForgeCmd::Forge(DynamicVars().Forge().IntValue(), Owner(), *this);
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};
}  // namespace sts2
