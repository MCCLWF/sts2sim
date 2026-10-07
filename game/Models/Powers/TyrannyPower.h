#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct TyrannyPower : PowerModel {
    SIM_MODEL(TyrannyPower, PowerModel)
    Dec ModifyHandDraw(Player player, Dec count) const {
        return player == Owner().Player() ? count + Dec(Amount()) : count;
    }
    void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const {
        if (player != Owner().Player()) return;
        CardList selected = CardSelectCmd::FromHand(ctx, player, CardSelectorPrefs(Amount()), nullptr, *this);
        for (CardModel card : selected) CardCmd::Exhaust(ctx, card);
    }
};
}  // namespace sts2
