// Models/Cards/Decay.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Decay : CardModel {
    SIM_MODEL(Decay, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    bool HasTurnEndInHandEffect() const { return true; }
    void OnTurnEndInHand(PlayerChoiceContext& ctx) const {
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().Damage(), *this, nullptr);
    }
};

}  // namespace sts2
