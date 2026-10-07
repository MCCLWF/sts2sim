#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Entities/Creatures/DamageResult.h"
#include "game/Models/Monsters/CeremonialBeast.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct PlowPower : PowerModel {
    SIM_MODEL(PlowPower, PowerModel)

    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result, ValueProp,
                             Creature, CardModel) const {
        if (target != Owner() || result.UnblockedDamage <= 0 || Owner().CurrentHp() > Dec(Amount())) return;

        for (PowerModel power : Owner().Powers()) {
            if (power.InternallyAppliedPowerId() == PowerId::STRENGTH_POWER) PowerCmd::Remove(power);
        }
        PowerModel strength = Owner().GetPower(PowerId::STRENGTH_POWER);
        if (strength) PowerCmd::Remove(strength);

        auto beast = CeremonialBeast(Owner().Monster());
        beast.SetIsStunnedByPlowRemoval();
        CreatureCmd::Stun(Owner(), SIM_MOVE(CeremonialBeast, StunnedMove), "BEAST_CRY_MOVE");
        PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
