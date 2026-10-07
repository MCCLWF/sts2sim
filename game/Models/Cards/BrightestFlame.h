#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct BrightestFlame : CardModel {
    SIM_MODEL(BrightestFlame, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        CreatureCmd::LoseMaxHp(ctx, Owner().Creature(), DynamicVars().MaxHp().BaseValue(), true);
    }

};

}  // namespace sts2
