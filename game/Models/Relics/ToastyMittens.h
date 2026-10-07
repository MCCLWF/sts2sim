// Models/Relics/ToastyMittens.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct ToastyMittens : RelicModel {
    SIM_MODEL(ToastyMittens, RelicModel)

    void AfterPlayerTurnStart(PlayerChoiceContext& context, Player player) const {
        if (player != Owner()) return;
        Flash();
        CardList selected = CardSelectCmd::FromHand(context, player, CardSelectorPrefs(1), nullptr, *this);
        for (CardModel card : selected) CardCmd::Exhaust(context, card);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, context, player.Creature(), DynamicVars().Strength().BaseValue(),
                        player.Creature(), CardModel{});
    }
};

}  // namespace sts2
