#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct BladeSymphony : CardModel {
    SIM_MODEL(BladeSymphony, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        sts2::CombatState combat = this->CombatState();
        if (!combat) return;
        for (Creature teammate : combat.GetTeammatesOf(Owner().Creature())) {
            if (!teammate.IsAlive() || !teammate.IsPlayer()) continue;
            Player player = teammate.Player();
            for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i) {
                CardModel shiv = combat.CreateCard(CardId::SHIV, player);
                CardPileCmd::AddGeneratedCardToCombat(shiv, PileType::Hand, player);
            }
        }
    }
};

}  // namespace sts2
