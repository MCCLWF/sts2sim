#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct BladeOfInk : CardModel {
    SIM_MODEL(BladeOfInk, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        CardList shivs;
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i) {
            shivs.push_back(CombatState().CreateCard(CardId::SHIV, Owner()));
        }
        CardPileCmd::AddGeneratedCardsToCombat(shivs, PileType::Hand, Owner());
        for (CardModel shiv : shivs) CardCmd::Enchant(EnchantmentId::INKY, shiv, Dec(1));
    }
};

}  // namespace sts2
