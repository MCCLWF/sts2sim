// Models/Cards/EscapePlan.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct EscapePlan : CardModel {
    SIM_MODEL(EscapePlan, CardModel)

    bool GainsBlock() const { return true; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* cardPlay) const {
        CardList drawn = CardPileCmd::Draw(ctx, Dec(1), Owner());
        if (!drawn.empty() && drawn[0].Type() == CardType::Skill) {
            CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), cardPlay);
        }
    }
};

}  // namespace sts2
