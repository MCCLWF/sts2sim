// Models/Cards/LeadingStrike.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Combat/CombatManager.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct LeadingStrike : CardModel {
    SIM_MODEL(LeadingStrike, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .FromCard(*this, play)
            .Targeting(play->Target)
            .Execute(ctx);

        const int32_t shivCount = DynamicVars()["Shivs"].IntValue();
        for (int32_t i = 0; i < shivCount; ++i) {
            // Shiv.CreateInHand(owner, combatState) is called once per loop
            // iteration in the original, so each generated hook resolves
            // before the next Shiv is created.
            if (CombatManager(&sim()).IsOverOrEnding()) break;
            sts2::CombatState combatState = CombatState();
            CardModel shiv = combatState.CreateCard(CardId::SHIV, Owner());
            CardPileCmd::AddGeneratedCardToCombat(shiv, PileType::Hand, Owner());
        }
    }
};

}  // namespace sts2
