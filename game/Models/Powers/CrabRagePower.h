// Models/Powers/CrabRagePower.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct CrabRagePower : PowerModel {
    SIM_MODEL(CrabRagePower, PowerModel)
    void AfterDeath(PlayerChoiceContext& ctx, Creature creature, bool, float) const {
        if (creature != Owner() && creature.Side() == Owner().Side()) {
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), DynamicVars().Strength().BaseValue(), Owner(), CardModel{});
            CreatureCmd::GainBlock(Owner(), DynamicVars().Block().BaseValue(), ValueProp::Unpowered, nullptr);
            PowerCmd::Remove(*this);
        }
    }
};
}  // namespace sts2
