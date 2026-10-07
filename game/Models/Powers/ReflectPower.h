#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {
struct ReflectPower : PowerModel {
    SIM_MODEL(ReflectPower, PowerModel)
    void AfterDamageReceived(PlayerChoiceContext& ctx, Creature target, const DamageResult& result, ValueProp props, Creature dealer, CardModel) const {
        if (target == Owner() && result.BlockedDamage > Dec(0) && IsPoweredAttack(props) && dealer)
            CreatureCmd::Damage(ctx, dealer, result.BlockedDamage, ValueProp::Unpowered, Owner());
    }
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner())) PowerCmd::Decrement(*this);
    }
};
}  // namespace sts2
