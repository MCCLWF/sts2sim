// Models/Cards/Shame.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Shame : CardModel {
    SIM_MODEL(Shame, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    bool HasTurnEndInHandEffect() const { return true; }
    void OnTurnEndInHand(PlayerChoiceContext& ctx) const {
        bool already = Owner().Creature().HasPower(PowerId::FRAIL_POWER);
        PowerModel power = PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, Owner().Creature(), DynamicVars()["Frail"].BaseValue(),
                                           Creature{}, *this);
        if (power && !already) power.SetSkipNextDurationTick(true);
    }
};

}  // namespace sts2
