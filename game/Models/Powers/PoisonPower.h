// Models/Powers/PoisonPower.cs
#pragma once

#include <algorithm>
#include "core/csharp_int.h"

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct PoisonPower : PowerModel {
    SIM_MODEL(PoisonPower, PowerModel)

    int32_t TriggerCount() const {
        int32_t total = 0;
        sts2::CombatState combatState = Owner().CombatState();
        for (Creature opponent : combatState.GetOpponentsOf(Owner())) {
            if (!opponent.IsAlive()) continue;
            PowerModel accelerant = opponent.GetPower(PowerId::ACCELERANT_POWER);
            if (accelerant) total = checked_int_add(total, accelerant.Amount());
        }
        return std::min(int_add(1, total), Amount());
    }

    void Trigger(PlayerChoiceContext& ctx) const {
        const int32_t trigger_count = TriggerCount();
        for (int32_t i = 0; i < trigger_count; ++i) {
            ExecutionSourceScope damageSource(Owner().sim(), ExecutionDamageSourceKind::Poison, "PoisonPower");
            CreatureCmd::Damage(ctx, Owner(), Dec(Amount()), ValueProp::Unblockable | ValueProp::Unpowered,
                                Creature{}, CardModel{}, nullptr);
            if (Owner().IsAlive()) PowerCmd::Decrement(*this);
        }
    }

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (!participants.contains(Owner())) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        Trigger(ctx);
    }
};

}  // namespace sts2
