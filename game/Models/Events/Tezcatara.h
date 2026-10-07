#pragma once

#include "game/Models/CardModel.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct Tezcatara : EventModel {
    SIM_MODEL(Tezcatara, EventModel)

    EventOptionList GenerateInitialOptions() const {
        // OptionPool1.ToList(): materialize both relics before the first draw.
        EventOptionList first;
        first.push_back(AncientRelicOption(RelicId::VERY_HOT_COCOA));
        first.push_back(AncientRelicOption(RelicId::YUMMY_COOKIE));

        bool hasBasicStrike = false;
        for (CardModel card : Owner().Deck().Cards()) {
            if (card.HasTag(CardTag::Strike) && card.Rarity() == CardRarity::Basic) {
                hasBasicStrike = true;
                break;
            }
        }
        if (hasBasicStrike) first.push_back(AncientRelicOption(RelicId::NUTRITIOUS_SOUP));

        EventOptionList result;
        result.push_back(Rng().NextItem(first));

        // Build the full second pool before drawing from it.
        EventOptionList second;
        second.push_back(AncientRelicOption(RelicId::BIIIG_HUG));
        second.push_back(AncientRelicOption(RelicId::STORYBOOK));
        second.push_back(AncientRelicOption(RelicId::TOASTY_MITTENS));
        result.push_back(Rng().NextItem(second));

        // Build the full third pool before its draw as well.
        EventOptionList third;
        third.push_back(AncientRelicOption(RelicId::GOLDEN_COMPASS));
        third.push_back(AncientRelicOption(RelicId::PUMPKIN_CANDLE));
        third.push_back(AncientRelicOption(RelicId::TOY_BOX));
        third.push_back(AncientRelicOption(RelicId::SEAL_OF_GOLD));
        result.push_back(Rng().NextItem(third));

        return result;
    }
};

}  // namespace sts2
