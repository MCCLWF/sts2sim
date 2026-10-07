#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/Monsters/BowlbugRock.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ImbalancedPower : PowerModel {
    SIM_MODEL(ImbalancedPower, PowerModel)

    void AfterDamageGiven(PlayerChoiceContext&, Creature dealer, const DamageResult& result,
                          ValueProp, Creature, CardModel) const {
        if (dealer != Owner() || !result.WasFullyBlocked) return;
        if (Owner().Monster() && Owner().Monster().MonsterId_() == MonsterId::BOWLBUG_ROCK) {
            BowlbugRock(Owner().Monster()).SetIsOffBalance(true);
        } else {
            CreatureCmd::Stun(Owner());
        }
    }
};

}  // namespace sts2
