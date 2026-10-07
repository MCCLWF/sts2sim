// Models/Cards/Bash.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Bash : CardModel {
    SIM_MODEL(Bash, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (!cardPlay->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, cardPlay).Targeting(cardPlay->Target).Execute(choiceContext);
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, choiceContext, cardPlay->Target, DynamicVars().Vulnerable().BaseValue(),
                        Owner().Creature(), *this);
    }
};

}  // namespace sts2
