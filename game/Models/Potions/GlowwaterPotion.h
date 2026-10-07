// Models/Potions/GlowwaterPotion.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct GlowwaterPotion : PotionModel {
    SIM_MODEL(GlowwaterPotion, PotionModel)

    void OnUse(PlayerChoiceContext& choiceContext, Creature target) const {
        if (!target) return;
        Player player = target.Player();
        CardList hand = player.PlayerCombatState().Hand().Cards();
        for (CardModel card : hand) CardCmd::Exhaust(choiceContext, card);
        CardPileCmd::Draw(choiceContext, DynamicVars().Cards().BaseValue(), player);
    }
};

}  // namespace sts2
