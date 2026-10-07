#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct OneForAll : CardModel { SIM_MODEL(OneForAll, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        for (Player player : CombatState().Players())
            PowerCmd::Apply(PowerId::ONE_FOR_ALL_POWER, ctx, player.Creature(), DynamicVars()["OneForAllPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
