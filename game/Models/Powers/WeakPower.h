// Models/Powers/WeakPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/Models/Relics/PaperKrane.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct WeakPower : PowerModel {
    SIM_MODEL(WeakPower, PowerModel)

    Dec ModifyDamageMultiplicative(Creature target, Dec amount, ValueProp props, Creature dealer, CardModel cardSource,
                                   const CardPlay* cardPlay) const {
        if (dealer != Owner()) return Dec(1);
        if (!IsPoweredAttack(props)) return Dec(1);
        Dec num = DynamicVars()["DamageDecrease"].BaseValue();
        sts2::Player targetPlayer = target ? target.Player() : sts2::Player{};
        if (targetPlayer) {
            RelicModel relic = targetPlayer.GetRelic(RelicId::PAPER_KRANE);
            if (relic) num = PaperKrane(sim_, relic.Index()).ModifyWeakMultiplier(target, num, props, dealer, cardSource);
        }
        if (PowerModel debilitate = dealer.GetPower(PowerId::DEBILITATE_POWER))
            num = DebilitatePower(sim_, debilitate.Index()).ModifyWeakMultiplier(dealer, num, props, dealer, cardSource);
        return num;
    }

    void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide side, const CreatureList& participants) const {
        if (side == CombatSide::Enemy) PowerCmd::TickDownDuration(*this);
    }
};

}  // namespace sts2
