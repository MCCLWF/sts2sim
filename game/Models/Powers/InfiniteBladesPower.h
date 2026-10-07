#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct InfiniteBladesPower : PowerModel {
    SIM_MODEL(InfiniteBladesPower, PowerModel)

    void BeforeHandDraw(Player player, PlayerChoiceContext&, sts2::CombatState combatState) const {
        if (player != Owner().Player()) return;
        for (int32_t i = 0; i < Amount(); ++i) {
            CardModel shiv = combatState.CreateCard(CardId::SHIV, player);
            CardPileCmd::AddGeneratedCardToCombat(shiv, PileType::Hand, player);
        }
    }
};

}  // namespace sts2
