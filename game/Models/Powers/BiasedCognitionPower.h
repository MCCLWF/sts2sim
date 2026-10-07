// Models/Powers/BiasedCognitionPower.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct BiasedCognitionPower : PowerModel { SIM_MODEL(BiasedCognitionPower, PowerModel)
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (!participants.contains(Owner())) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, Owner(), Dec(-Amount()), Owner(), CardModel{});
    }
};
}
