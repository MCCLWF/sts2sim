// Models/Relics/GremlinHorn.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct GremlinHorn : RelicModel {
    SIM_MODEL(GremlinHorn, RelicModel)

    void AfterDeath(PlayerChoiceContext& ctx, Creature target, bool, float) const {
        if (target.Side() == Owner().Creature().Side()) return;
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};

}  // namespace sts2
