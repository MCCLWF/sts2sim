// Models/Relics/Claws.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Claws : RelicModel {
    SIM_MODEL(Claws, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    static CardModel CreateMaulFromOriginal(CardModel original) {
        CardModel maul = original.Owner().RunState().CreateCard(CardId::MAUL, original.Owner());
        if (original.CurrentUpgradeLevel() > 0 && maul.IsUpgradable()) CardCmd::Upgrade(maul);

        EnchantmentModel enchantment = original.Enchantment();
        if (enchantment && CardCmd::CanEnchant(enchantment.EnchantmentId_(), maul))
            CardCmd::CloneEnchantment(original, maul);
        return maul;
    }

    void AfterObtained() const {
        CardList selected = CardSelectCmd::FromDeckForTransformation(
            Owner(), CardSelectorPrefs(0, DynamicVars().Cards().IntValue()));
        std::vector<CardTransformation> transformations;
        transformations.reserve(selected.size());
        for (CardModel card : selected) {
            transformations.emplace_back(card, CreateMaulFromOriginal(card));
        }
        CardCmd::Transform(transformations, &Owner().PlayerRng().Transformations());
    }
};

}  // namespace sts2
