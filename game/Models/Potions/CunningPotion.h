#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct CunningPotion : PotionModel {
    SIM_MODEL(CunningPotion, PotionModel)

    void OnUse(PlayerChoiceContext&, Creature target) const {
        if (!target || !target.Player()) throw GameError("CunningPotion: target is not a player");
        Player player = target.Player();
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i) {
            CardModel shiv = target.CombatState().CreateCard(CardId::SHIV, player);
            CardPileCmd::AddGeneratedCardToCombat(shiv, PileType::Hand, player);
            CardCmd::Upgrade(shiv);
        }
    }
};

}  // namespace sts2
