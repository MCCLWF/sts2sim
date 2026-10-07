#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Entities/Creatures/DamageResult.h"
#include "game/Models/Monsters/SlumberingBeetle.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct SlumberPower : PowerModel {
    SIM_MODEL(SlumberPower, PowerModel)

    void AfterRemoved(Creature) const {}

    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result, ValueProp, Creature,
                             CardModel) const {
        if (target != Owner() || result.UnblockedDamage == 0) return;
        PowerCmd::Decrement(*this);
        if (Amount() <= 0 && Owner().Monster() && Owner().Monster().MonsterId_() == MonsterId::SLUMBERING_BEETLE) {
            CreatureCmd::Stun(Owner(), SIM_MOVE(SlumberingBeetle, WakeUpMove), "ROLL_OUT_MOVE");
        }
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        PowerCmd::Decrement(*this);
        if (Amount() <= 0 && Owner().Monster() && Owner().Monster().MonsterId_() == MonsterId::SLUMBERING_BEETLE) {
            SlumberingBeetle(Owner().Monster()).WakeUpMove(CreatureList{});
        }
    }
};

}  // namespace sts2
