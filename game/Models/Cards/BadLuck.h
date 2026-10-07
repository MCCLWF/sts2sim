#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct BadLuck : CardModel {
    SIM_MODEL(BadLuck, CardModel)

    bool CanBeGeneratedByModifiers() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }
    bool HasTurnEndInHandEffect() const { return true; }

    void OnTurnEndInHand(PlayerChoiceContext& ctx) const {
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().HpLoss().BaseValue(),
                            ValueProp::Unblockable | ValueProp::Unpowered | ValueProp::Move, *this, nullptr);
    }
};

}  // namespace sts2
