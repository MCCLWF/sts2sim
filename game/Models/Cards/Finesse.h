// Models/Cards/Finesse.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Finesse : CardModel {
    SIM_MODEL(Finesse, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};

}  // namespace sts2
