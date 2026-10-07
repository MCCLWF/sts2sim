#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct DecisionsDecisions : CardModel {
    SIM_MODEL(DecisionsDecisions, CardModel)
    static bool IsPlayableSkill(CardModel c) { return c.Type() == CardType::Skill && !c.Keywords().Contains(CardKeyword::Unplayable); }
    int32_t CanonicalStarCost() const { return 6; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        CardList selection = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(1), IsPlayableSkill, *this);
        if (selection.empty()) return;
        for (int32_t i = 0; i < DynamicVars().Repeat().IntValue(); ++i)
            CardCmd::AutoPlay(ctx, selection[0], Creature{});
    }
};
}  // namespace sts2
