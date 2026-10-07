// Models/Cards/Radiate.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Radiate : CardModel {
    SIM_MODEL(Radiate, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        int32_t hits = DynamicVars()["CalculatedHits"].Calculate(cardPlay ? cardPlay->Target : Creature{}).to_int();
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .WithHitCount(hits)
            .FromCard(*this, cardPlay)
            .TargetingAllOpponents(CombatState())
            .Execute(choiceContext);
    }
};

}  // namespace sts2
