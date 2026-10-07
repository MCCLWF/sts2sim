// Models/Events/Pael.cs
#pragma once

#include <cstddef>

#include "game/Models/CardModel.h"
#include "game/Models/EventModel.h"
#include "game/Models/Events/AllowedHelpers.h"

namespace sts2 {

struct Pael : EventModel {
    SIM_MODEL(Pael, EventModel)

    // Goopy.CanEnchant is EnchantmentModel.CanEnchant plus its Defend-tag
    // restriction. Goopy itself is non-stackable, as reflected by the base
    // helper's rejection of cards that already have an enchantment.
    bool CanEnchantWithGoopy(CardModel card) const {
        return event_allowed::BaseCanEnchant(card) && card.HasTag(CardTag::Defend);
    }

    // Player.HasEventPet checks RelicModel.AddsPet and, only if no such relic
    // exists, looks for a ByrdonisEgg in the deck. In v0.111.0 the only relics
    // overriding AddsPet are Byrdpip and PaelsLegion.
    bool HasEventPet() const {
        for (RelicModel relic : Owner().Relics()) {
            if (relic.RelicId_() == RelicId::BYRDPIP || relic.RelicId_() == RelicId::PAELS_LEGION) return true;
        }
        for (CardModel card : Owner().Deck().Cards()) {
            if (card.CardId_() == CardId::BYRDONIS_EGG) return true;
        }
        return false;
    }

    EventOptionList GenerateInitialOptions() const {
        // OptionPool1 creates all three mutable relic options before drawing.
        EventOptionList first;
        first.push_back(AncientRelicOption(RelicId::PAELS_FLESH));
        first.push_back(AncientRelicOption(RelicId::PAELS_HORN));
        first.push_back(AncientRelicOption(RelicId::PAELS_TEARS));
        EventOptionState firstChoice = Rng().NextItem(first);

        // OptionPool2 starts with Pael's Wing. The two eligibility counts run
        // in source order, then List.AddRange(list) duplicates every entry
        // already present, including the same relic-option references.
        EventOptionList second;
        second.push_back(AncientRelicOption(RelicId::PAELS_WING));

        int32_t goopyEligible = 0;
        for (CardModel card : Owner().Deck().Cards()) {
            if (CanEnchantWithGoopy(card)) ++goopyEligible;
        }
        if (goopyEligible >= 3) second.push_back(AncientRelicOption(RelicId::PAELS_CLAW));

        int32_t removable = 0;
        for (CardModel card : Owner().Deck().Cards()) {
            if (card.IsRemovable()) ++removable;
        }
        if (removable >= 5) second.push_back(AncientRelicOption(RelicId::PAELS_TOOTH));

        const std::size_t entriesBeforeSelfAdd = second.size();
        for (std::size_t i = 0; i < entriesBeforeSelfAdd; ++i) second.push_back(second[i]);
        second.push_back(AncientRelicOption(RelicId::PAELS_GROWTH));
        EventOptionState secondChoice = Rng().NextItem(second);

        // OptionPool3 is built after the second draw. Player.HasEventPet then
        // determines whether Pael's Legion is appended before the third draw.
        EventOptionList third;
        third.push_back(AncientRelicOption(RelicId::PAELS_EYE));
        third.push_back(AncientRelicOption(RelicId::PAELS_BLOOD));
        if (!HasEventPet()) third.push_back(AncientRelicOption(RelicId::PAELS_LEGION));
        EventOptionState thirdChoice = Rng().NextItem(third);

        EventOptionList result;
        result.push_back(firstChoice);
        result.push_back(secondChoice);
        result.push_back(thirdChoice);
        return result;
    }
};

}  // namespace sts2
