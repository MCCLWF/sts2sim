#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Wish : CardModel {
    SIM_MODEL(Wish, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList selected = CardSelectCmd::FromCombatPile(ctx, Owner().PlayerCombatState().DrawPile(), Owner(),
                                                           CardSelectorPrefs(1));
        if (!selected.empty()) CardPileCmd::Add(selected[0], PileType::Hand);
    }

};

}  // namespace sts2
