// Models/Events/RoomFullOfCheese.cs (IsAllowed partial port)
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/EventModel.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct RoomFullOfCheese : EventModel {
    SIM_MODEL(RoomFullOfCheese, EventModel)
    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(RoomFullOfCheese, Gorge), InitialOptionKey("GORGE")));
        list.push_back(EventOption(SIM_OPTION(RoomFullOfCheese, Search), InitialOptionKey("SEARCH"))
                           .ThatDoesDamage(DynamicVars().Damage().BaseValue()));
        return list;
    }
    void Gorge(const EventOptionState&) const {
        CardCreationOptionsState options = CardCreationOptions::ForNonCombatWithUniformOdds({sts2::data(Owner().Character()).card_pool});
        options.flags = options.flags | CardCreationFlags::NoRarityModification;
        options.required_rarity = CardRarity::Common;
        CardList candidates = CardFactory::CreateForReward(Owner(), 8, options);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CardList selected = CardSelectCmd::FromSimpleGridForRewards(ctx, candidates, Owner(), CardSelectorPrefs(2));
        for (CardModel card : selected) CardPileCmd::Add(card, PileType::Deck);
        SetEventFinished(L10NLookup("ROOM_FULL_OF_CHEESE.pages.GORGE.description"));
    }
    void Search(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().Damage(), Creature{});
        RelicCmd::Obtain(RelicId::CHOSEN_CHEESE, Owner());
        SetEventFinished(L10NLookup("ROOM_FULL_OF_CHEESE.pages.SEARCH.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return runState.CurrentActIndex() < 2; }
};

}  // namespace sts2
