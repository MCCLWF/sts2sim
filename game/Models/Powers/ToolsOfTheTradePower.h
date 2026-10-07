#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ToolsOfTheTradePower : PowerModel {
    SIM_MODEL(ToolsOfTheTradePower, PowerModel)

    Dec ModifyHandDraw(Player player, Dec count) const {
        return player == Owner().Player() ? count + Dec(Amount()) : count;
    }

    void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const {
        if (player != Owner().Player()) return;
        CardList selected = CardSelectCmd::FromHandForDiscard(ctx, player, CardSelectorPrefs(Amount()), nullptr, *this);
        if (!selected.empty()) CardCmd::Discard(ctx, selected);
    }
};

}  // namespace sts2
