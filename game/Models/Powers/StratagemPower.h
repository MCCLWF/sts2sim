#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct StratagemPower : PowerModel {
    SIM_MODEL(StratagemPower, PowerModel)

    void AfterShuffle(PlayerChoiceContext& ctx, Player player) const {
        if (player != Owner().Player()) return;
        Flash();
        CardList selected = CardSelectCmd::FromCombatPile(ctx, player.PlayerCombatState().DrawPile(), player,
                                                          CardSelectorPrefs(Amount()));
        for (CardModel card : selected) CardPileCmd::Add(card, PileType::Hand);
    }
};

}  // namespace sts2
