#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct NinjaScroll : RelicModel {
    SIM_MODEL(NinjaScroll, RelicModel)

    void BeforeHandDraw(Player player, PlayerChoiceContext&, CombatState combatState) const {
        if (player != Owner() || player.PlayerCombatState().TurnNumber() > 1) return;
        for (int32_t i = 0; i < DynamicVars()["Shivs"].IntValue(); ++i) {
            CardModel shiv = combatState.CreateCard(CardId::SHIV, player);
            CardPileCmd::AddGeneratedCardToCombat(shiv, PileType::Hand, player);
        }
    }
};

}  // namespace sts2
