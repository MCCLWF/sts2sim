// Models/Relics/GamblingChip.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct GamblingChip : RelicModel {
    SIM_MODEL(GamblingChip, RelicModel)

    void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const {
        if (player != Owner() || Owner().PlayerCombatState().TurnNumber() > 1) return;
        CardList cards = CardSelectCmd::FromHandForDiscard(ctx, Owner(), CardSelectorPrefs(0, 999999999), nullptr, *this);
        if (!cards.empty()) CardCmd::DiscardAndDraw(ctx, cards, static_cast<int32_t>(cards.size()));
    }
};

}  // namespace sts2
