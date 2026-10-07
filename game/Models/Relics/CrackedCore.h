// Models/Relics/CrackedCore.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct CrackedCore : RelicModel {
    SIM_MODEL(CrackedCore, RelicModel)
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature()) && Owner().PlayerCombatState().TurnNumber() <= 1) {
            PlayerChoiceContext blocking;
            for (int32_t i = 0; Dec(i) < DynamicVars()["Lightning"].BaseValue(); ++i)
                OrbCmd::Channel(OrbId::LIGHTNING_ORB, blocking, Owner());
        }
    }
};
}
