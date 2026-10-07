#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct RadiantTincture : PotionModel {
    SIM_MODEL(RadiantTincture, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) return;
        PlayerCmd::GainEnergy(DynamicVars().Energy().IntValue(), target.Player());
        PowerCmd::Apply(PowerId::RADIANCE_POWER, ctx, target, DynamicVars()["RadiancePower"].BaseValue(),
                        Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
