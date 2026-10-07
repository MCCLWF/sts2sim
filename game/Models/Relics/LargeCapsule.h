// Models/Relics/LargeCapsule.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Factories/RelicFactory.h"
#include "game/Commands/RelicCmd.h"
#include "game/Commands/CardPileCmd.h"
namespace sts2 {
struct LargeCapsule : RelicModel {
    SIM_MODEL(LargeCapsule, RelicModel)
    CardId BasicForCharacter(CardTag tag) const {
        for (CardId id : sts2::data(sts2::data(Owner().Character()).card_pool).cards) {
            const CardData& card = sts2::data(id);
            if (card.rarity == CardRarity::Basic && ((card.tags >> static_cast<unsigned>(tag)) & 1u))
                return id;
        }
        throw GameError("LargeCapsule: character pool has no matching basic card");
    }
    void AfterObtained() const {
        for (int i = 0; i < DynamicVars()["Relics"].IntValue(); ++i)
            RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
        CardPileCmd::Add(Owner().RunState().CreateCard(BasicForCharacter(CardTag::Strike), Owner()), PileType::Deck);
        CardPileCmd::Add(Owner().RunState().CreateCard(BasicForCharacter(CardTag::Defend), Owner()), PileType::Deck);
    }
};
}
