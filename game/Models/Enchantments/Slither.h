// Models/Enchantments/Slither.cs
#pragma once

#include "game/Models/EnchantmentModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Slither : EnchantmentModel {
    SIM_MODEL(Slither, EnchantmentModel)
    bool CanEnchant(CardModel card) const {
        return card && !card.Keywords().Contains(CardKeyword::Unplayable) && !card.EnergyCost().CostsX();
    }
    void AfterCardDrawn(PlayerChoiceContext&, CardModel card, bool) const {
        if (card != Card() || !card.Pile() || card.Pile().Type() != PileType::Hand) return;
        card.EnergyCost().SetThisCombat(RunState(&card.sim()).Rng().CombatEnergyCosts().NextInt(4));
    }
};

}  // namespace sts2
