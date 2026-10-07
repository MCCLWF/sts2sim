// Models/Events/DoorsOfLightAndDark.cs
#pragma once
#include <algorithm>
#include "core/dotnet_sort.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/EventModel.h"
namespace sts2 {
struct DoorsOfLightAndDark : EventModel {
    SIM_MODEL(DoorsOfLightAndDark, EventModel)
    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(DoorsOfLightAndDark, Light), InitialOptionKey("LIGHT")));
        list.push_back(EventOption(SIM_OPTION(DoorsOfLightAndDark, Dark), InitialOptionKey("DARK")));
        return list;
    }
    void Light(const EventOptionState&) const {
        CardList cards;
        for (CardModel card : Owner().Deck().Cards()) if (card.IsUpgradable()) cards.push_back(card);
        dotnet_sort(cards.span(), [](CardModel a, CardModel b) { return a.CompareTo(b); });
        Rng().Shuffle(cards);
        int32_t count = std::min<int32_t>(DynamicVars().Cards().IntValue(), cards.size());
        for (int32_t i = 0; i < count; ++i) CardCmd::Upgrade(cards[static_cast<size_t>(i)]);
        SetEventFinished(L10NLookup("DOORS_OF_LIGHT_AND_DARK.pages.LIGHT.description"));
    }
    void Dark(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(1));
        CardPileCmd::RemoveFromDeck(cards);
        SetEventFinished(L10NLookup("DOORS_OF_LIGHT_AND_DARK.pages.DARK.description"));
    }
};
}
