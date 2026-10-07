#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct Clarity : PotionModel {
    SIM_MODEL(Clarity, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) return;
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), target.Player());
        PowerCmd::Apply(PowerId::CLARITY_POWER, ctx, target, DynamicVars()["ClarityPower"].BaseValue(),
                        Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
