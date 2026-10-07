#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct DoubleDamagePower : PowerModel {
    SIM_MODEL(DoubleDamagePower, PowerModel)

    Dec ModifyDamageMultiplicative(Creature, Dec, ValueProp props, Creature dealer, CardModel cardSource,
                                   const CardPlay*) const {
        if (!dealer || !IsPoweredAttack(props) || !cardSource) return Dec(1);
        if (dealer == Owner()) return Dec(2);
        Player player = Owner().Player();
        if (player && player.PlayerCombatState().Pets().contains(dealer)) return Dec(2);
        return Dec(1);
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
