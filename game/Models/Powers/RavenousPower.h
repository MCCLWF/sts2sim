// Models/Powers/RavenousPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/Monsters/CorpseSlug.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct RavenousPower : PowerModel {
    SIM_MODEL(RavenousPower, PowerModel)

    void AfterDeath(PlayerChoiceContext& choiceContext, Creature target, bool wasRemovalPrevented, float deathAnimLength) const {
        if (!wasRemovalPrevented && target != Owner() && target.Side() == Owner().Side() && !Owner().IsDead()) {
            Flash();
            CorpseSlug(Owner().Monster()).SetIsRavenous(true);
            CreatureCmd::Stun(Owner(), STS2_NATIVE_CALLBACK(Move, RavenousPower, (&StunnedMove), "StunnedMove"));
            PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, Owner(), Dec(Amount()), Owner(), CardModel{});
        }
    }

    // StunnedMove: runs as the owner's move, so it receives the owner's monster.
    static void StunnedMove(MonsterModel owner, const CreatureList& targets) { CorpseSlug(owner).SetIsRavenous(false); }
};

}  // namespace sts2
