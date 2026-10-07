// Models/Cards/Doubt.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Doubt : CardModel {
    SIM_MODEL(Doubt, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    bool HasTurnEndInHandEffect() const { return true; }
    void OnTurnEndInHand(PlayerChoiceContext& ctx) const {
        bool already = Owner().Creature().HasPower(PowerId::WEAK_POWER);
        PowerModel power = PowerCmd::Apply(PowerId::WEAK_POWER, ctx, Owner().Creature(),
                                           DynamicVars().Weak().BaseValue(), Creature{}, *this);
        if (power && !already) power.SetSkipNextDurationTick(true);
    }
};

}  // namespace sts2
