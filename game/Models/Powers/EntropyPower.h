#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct EntropyPower : PowerModel {
    SIM_MODEL(EntropyPower, PowerModel)

    void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const {
        if (player != Owner().Player()) return;
        CardList selected = CardSelectCmd::FromHand(ctx, player, CardSelectorPrefs(Amount()), nullptr, *this);
        for (CardModel card : selected) CardCmd::TransformToRandom(card, Owner().Player().RunState().Rng().CombatCardSelection());
    }
};

}  // namespace sts2
