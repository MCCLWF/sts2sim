#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct HailstormPower : PowerModel { SIM_MODEL(HailstormPower, PowerModel)
    void BeforeSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        int32_t frosts = 0;
        for (OrbRef ref : Owner().Player().PlayerCombatState().st().orbs)
            if (OrbModel(sim_, ref).OrbId_() == OrbId::FROST_ORB) ++frosts;
        if (frosts >= DynamicVars()["FrostOrbs"].IntValue())
            CreatureCmd::Damage(ctx, Owner().CombatState().HittableEnemies(), Dec(Amount()), ValueProp::Unpowered, Owner());
    }
};
}
