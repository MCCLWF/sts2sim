// Models/Relics/SymbioticVirus.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct SymbioticVirus : RelicModel { SIM_MODEL(SymbioticVirus, RelicModel)
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner().Creature()) && Owner().PlayerCombatState().TurnNumber() <= 1) {
            PlayerChoiceContext ctx;
            for (int32_t i = 0; Dec(i) < DynamicVars()["Dark"].BaseValue(); ++i)
                OrbCmd::Channel(OrbId::DARK_ORB, ctx, Owner());
        }
    }
};
}
