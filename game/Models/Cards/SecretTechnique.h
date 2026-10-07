#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct SecretTechnique : CardModel {
    SIM_MODEL(SecretTechnique, CardModel)

    static bool IsSkill(CardModel card) { return card.Type() == CardType::Skill; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList cards = CardSelectCmd::FromCombatPile(ctx, Owner().PlayerCombatState().DrawPile(), Owner(),
                                                        CardSelectorPrefs(1), IsSkill);
        if (!cards.empty()) CardPileCmd::Add(cards.front(), PileType::Hand);
    }
};

}  // namespace sts2
