// Models/Relics/HeftyTablet.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/Canonical.h"
namespace sts2 {
struct HeftyTablet : RelicModel {
    SIM_MODEL(HeftyTablet, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const {
        CardList cards;
        CardPoolId pool = sts2::data(Owner().Character()).card_pool;
        for (CardId id : CardFactory::GetUnlockedCards(pool, Owner())) {
            CanonicalCard c(&sim(), id);
            if (c.get().Rarity() == CardRarity::Rare) cards.push_back(Owner().RunState().CreateCard(id, Owner()));
        }
        CardList options;
        for (int i = 0; i < 3 && !cards.empty(); ++i) {
            int32_t k = Owner().PlayerRng().Rewards().next_item_index(static_cast<int32_t>(cards.size()));
            options.push_back(cards[static_cast<size_t>(k)]);
            cards.erase_at(static_cast<size_t>(k));
        }
        PlayerChoiceContext context;
        CardModel chosen = CardSelectCmd::FromChooseACardScreen(context, options, Owner(), true);
        CardList add; if (chosen) add.push_back(chosen); add.push_back(Owner().RunState().CreateCard(CardId::INJURY, Owner())); CardPileCmd::Add(add, PileType::Deck);
    }
};
}
