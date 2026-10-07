// Models/Cards/Beckon.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Beckon : CardModel {
    SIM_MODEL(Beckon, CardModel)

    int32_t MaxUpgradeLevel() const { return 0; }
    bool HasTurnEndInHandEffect() const { return true; }

    void OnTurnEndInHand(PlayerChoiceContext& choiceContext) const {
        CreatureCmd::Damage(choiceContext, Owner().Creature(), DynamicVars().HpLoss().BaseValue(),
                            ValueProp::Unblockable | ValueProp::Unpowered | ValueProp::Move, *this, nullptr);
    }
};

}  // namespace sts2
