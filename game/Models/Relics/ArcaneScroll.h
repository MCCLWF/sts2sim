// Models/Relics/ArcaneScroll.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Models/Canonical.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Runs/CardCreationOptions.h"
namespace sts2 {
struct ArcaneScroll : RelicModel {
    SIM_MODEL(ArcaneScroll, RelicModel)
    void AfterObtained() const {
        CardCreationOptionsState o = CardCreationOptions::Make({CharacterCardPool()}, CardCreationSource::Other, CardRarityOddsType::Uniform);
        CardCreationOptions::WithFlags(o, CardCreationFlags::NoUpgradeRoll);
        CardList possible;
        for (CardId id : CardCreationOptions::GetPossibleCards(o, Owner())) {
            CanonicalCard c(&sim(), id);
            if (c.get().Rarity() == CardRarity::Rare) possible.push_back(Owner().RunState().CreateCard(id, Owner()));
        }
        if (!possible.empty()) {
            int32_t i = Owner().PlayerRng().Rewards().next_item_index(static_cast<int32_t>(possible.size()));
            CardPileCmd::Add(possible[static_cast<size_t>(i)], PileType::Deck);
        }
    }
private:
    CardPoolId CharacterCardPool() const { return sts2::data(Owner().Character()).card_pool; }
};
}
