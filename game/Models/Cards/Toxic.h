// Models/Cards/Toxic.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Toxic : CardModel {
    SIM_MODEL(Toxic, CardModel)

    int32_t MaxUpgradeLevel() const { return 0; }
    bool HasTurnEndInHandEffect() const { return true; }

    void OnTurnEndInHand(PlayerChoiceContext& choiceContext) const {
        CreatureCmd::Damage(choiceContext, Owner().Creature(), DynamicVars().Damage(), *this, nullptr);
    }
};

}  // namespace sts2
