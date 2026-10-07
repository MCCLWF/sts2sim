#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct RupturePower : PowerModel {
    SIM_MODEL(RupturePower, PowerModel)

    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.Owner().Creature() != Owner() || CombatState().CurrentSide() != Owner().Side()) return;
        CombatState().CaptureCardPlayPowerAmount(*this, play->Card, 0);
    }

    void AfterDamageReceived(PlayerChoiceContext& ctx, Creature target, const DamageResult& result,
                             ValueProp, Creature, CardModel card) const {
        if (target != Owner() || result.UnblockedDamage <= 0 || CombatState().CurrentSide() != Owner().Side()) return;
        if (card && CombatState().AddCardPlayPowerAmount(*this, card, Amount())) return;
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(Amount()), Owner(), CardModel{});
    }

    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner().Creature() != Owner()) return;
        int32_t amount = 0;
        if (CombatState().TakeCardPlayPowerAmount(*this, play->Card, amount))
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(amount), Owner(), CardModel{});
    }
};
}  // namespace sts2
