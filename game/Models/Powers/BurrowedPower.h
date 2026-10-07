// Models/Powers/BurrowedPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/Monsters/Tunneler.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct BurrowedPower : PowerModel {
    SIM_MODEL(BurrowedPower, PowerModel)

    bool ShouldClearBlock(Creature creature) const { return Owner() != creature; }

    void AfterBlockBroken(PlayerChoiceContext&, Creature target, Creature) const {
        if (target != Owner() || !target.Monster() || target.Monster().MonsterId_() != MonsterId::TUNNELER) return;
        Tunneler tunneler(target.Monster());
        tunneler.GetStunned();
        CreatureCmd::Stun(Owner(), SIM_MOVE(Tunneler, StillDizzyMove), "BITE_MOVE");
        PowerCmd::Remove(*this);
    }

    void AfterRemoved(Creature oldOwner) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::LoseBlock(ctx, oldOwner, Dec(999999999), Creature{});
    }
};

}  // namespace sts2
