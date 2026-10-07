// Models/Powers/AsleepPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Entities/Creatures/DamageResult.h"
#include "game/Models/Monsters/LagavulinMatriarch.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct AsleepPower : PowerModel {
    SIM_MODEL(AsleepPower, PowerModel)

    void AfterDamageReceived(PlayerChoiceContext&, sts2::Creature target, const DamageResult& result,
                             ValueProp, sts2::Creature, CardModel) const {
        if (target != Owner() || result.UnblockedDamage == 0) return;
        PowerModel plating = Owner().GetPower(PowerId::PLATING_POWER);
        if (plating) PowerCmd::Remove(plating);
        CreatureCmd::Stun(Owner(), SIM_MOVE(LagavulinMatriarch, WakeUpMove), "SLASH_MOVE");
        PowerCmd::Remove(*this);
    }

    void BeforeSideTurnEndVeryEarly(PlayerChoiceContext&, CombatSide,
                                    const CreatureList& participants) const {
        if (participants.contains(Owner()) && Amount() <= 1 && Owner().HasPower(PowerId::PLATING_POWER)) {
            PowerCmd::Remove(Owner().GetPower(PowerId::PLATING_POWER));
        }
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        PowerCmd::Decrement(*this);
        if (Amount() <= 0) LagavulinMatriarch(Owner().Monster()).WakeUpMove(CreatureList{});
    }
};

}  // namespace sts2
