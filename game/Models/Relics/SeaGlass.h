// Models/Relics/SeaGlass.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/CardCreationOptions.h"

namespace sts2 {

struct SeaGlass : RelicModel {
    SIM_MODEL(SeaGlass, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        const CharacterId character = static_cast<CharacterId>(extra(0));
        if (character == CharacterId::NONE) {
            // The original defaults console-awarded relics with no saved
            // CharacterId to Ironclad before creating the three card groups.
            extra(0) = static_cast<int32_t>(CharacterId::IRONCLAD);
        }
        const CharacterId configuredCharacter = static_cast<CharacterId>(extra(0));
        const CardPoolId pool = sts2::data(configuredCharacter).card_pool;
        CardList cards;
        constexpr CardRarity rarities[] = {CardRarity::Common, CardRarity::Uncommon, CardRarity::Rare};
        const int32_t cardsPerRarity = DynamicVars().Cards().IntValue() / 3;
        for (CardRarity rarity : rarities) {
            CardCreationOptionsState options = CardCreationOptions::Make(
                {pool}, CardCreationSource::Other, CardRarityOddsType::Uniform);
            options.required_rarity = rarity;
            options.flags = options.flags | CardCreationFlags::NoRarityModification |
                            CardCreationFlags::NoCardPoolModifications | CardCreationFlags::NoUpgradeRoll;
            CardList group = CardFactory::CreateForReward(Owner(), cardsPerRarity, options);
            for (CardModel card : group) cards.push_back(card);
        }
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        CardList selected = CardSelectCmd::FromSimpleGridForRewards(
            context, cards, Owner(), CardSelectorPrefs(0, static_cast<int32_t>(cards.size())));
        for (CardModel card : selected) CardPileCmd::Add(card, PileType::Deck);
    }
};

}  // namespace sts2
