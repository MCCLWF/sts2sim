// Models/Relics/InfusedCore.cs
#pragma once

#include "game/Commands/OrbCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Models/OrbModel.h"

namespace sts2 {

struct InfusedCore : RelicModel {
    SIM_MODEL(InfusedCore, RelicModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature()) || Owner().PlayerCombatState().TurnNumber() > 1) return;
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        for (int32_t i = 0; i < DynamicVars()["Lightning"].IntValue(); ++i)
            OrbCmd::Channel(OrbId::LIGHTNING_ORB, context, Owner());
    }

    Dec ModifyOrbValue(OrbModel orb, Dec value) const {
        if (orb.Owner() != Owner() || orb.OrbId_() != OrbId::LIGHTNING_ORB) return value;
        return value + DynamicVars()["ExtraDamage"].BaseValue();
    }
};

}  // namespace sts2
