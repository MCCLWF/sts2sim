// Models/Powers/NemesisPower.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct NemesisPower : PowerModel {
    SIM_MODEL(NemesisPower, PowerModel)
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        extra(0) = !extra(0);
        if (extra(0)) PowerCmd::Apply(PowerId::INTANGIBLE_POWER, ctx, Owner(), Dec(1), Owner(), CardModel{});
        else if (auto intangible = Owner().GetPower(PowerId::INTANGIBLE_POWER)) PowerCmd::Remove(intangible);
    }
};
}  // namespace sts2
