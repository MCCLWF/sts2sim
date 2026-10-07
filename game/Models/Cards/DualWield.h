#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct DualWield : CardModel {
    SIM_MODEL(DualWield, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList selected = CardSelectCmd::FromHand(
            ctx, Owner(), CardSelectorPrefs(1),
            [](CardModel card) { return card.Type() == CardType::Attack || card.Type() == CardType::Power; }, *this);
        if (selected.empty()) return;
        CardModel selection = selected[0];
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i) {
            CardModel copy = selection.CreateClone();
            CardPileCmd::AddGeneratedCardToCombat(copy, PileType::Hand, Owner());
        }
    }

};

}  // namespace sts2
