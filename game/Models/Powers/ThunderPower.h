#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct ThunderPower : PowerModel { SIM_MODEL(ThunderPower, PowerModel)
    void AfterOrbEvoked(PlayerChoiceContext& ctx, OrbModel orb, const CreatureList& targets) const {
        if (orb.Owner() != Owner().Player() || orb.OrbId_() != OrbId::LIGHTNING_ORB) return;
        CreatureList alive;
        for (Creature c : targets) if (c.IsAlive()) alive.push_back(c);
        if (!alive.empty()) CreatureCmd::Damage(ctx, alive, Dec(Amount()), ValueProp::Unpowered, Owner());
    }
};
}
