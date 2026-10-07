#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

// Models/Powers/TemporaryDexterityPower.cs. Silent's Anticipate, Fade and
// Helical Dart use the applied Dexterity stack as a temporary delta and undo
// the remaining amount when the owner's side turn ends.
struct TemporaryDexterityPower : PowerModel {
    SIM_MODEL(TemporaryDexterityPower, PowerModel)

    void BeforeApplied(Creature target, Dec amount, Creature applier, CardModel cardSource) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, target, amount, applier, cardSource, true);
    }

    void AfterPowerAmountChanged(PlayerChoiceContext& ctx, PowerModel power, Dec amount, Creature applier,
                                 CardModel cardSource) const {
        if (power != *this || amount == Dec(Amount())) return;
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Owner(), amount, applier, cardSource, true);
    }

    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        int32_t remaining = Amount();
        PowerCmd::Remove(*this);
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Owner(), Dec(0) - remaining, Owner(), CardModel{});
    }
};

}  // namespace sts2
