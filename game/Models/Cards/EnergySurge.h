#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct EnergySurge : CardModel { SIM_MODEL(EnergySurge, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        for (Creature teammate : CombatState().GetTeammatesOf(Owner().Creature()))
            if (teammate.IsAlive() && teammate.IsPlayer()) PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), teammate.Player());
    }
};
}
