// Models/Events/Wellspring.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct Wellspring : EventModel {
    SIM_MODEL(Wellspring, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(Wellspring, Bottle),
                                   "WELLSPRING.pages.INITIAL.options.BOTTLE"));
        list.push_back(EventOption(SIM_OPTION(Wellspring, Bathe),
                                   "WELLSPRING.pages.INITIAL.options.BATHE"));
        return list;
    }

    void Bottle(const EventOptionState&) const {
        PotionId potion_id = Owner().PlayerRng().Rewards().NextItem(PotionFactory::GetPotionOptions(Owner()));
        if (potion_id != PotionId::NONE) {
            StaticVec<Reward, 8> rewards;
            PotionModel potion(&sim(), alloc_potion(*sim_->g, potion_id, 0));
            rewards.push_back(Reward::NewPotionReward(Owner(), potion));
            RewardsCmd::OfferCustom(Owner(), rewards);
        }
        SetEventFinished(L10NLookup("WELLSPRING.pages.BOTTLE.description"));
    }

    void Bathe(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(1));
        CardPileCmd::RemoveFromDeck(cards);
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::GUILTY, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("WELLSPRING.pages.BATHE.description"));
    }
};

}  // namespace sts2
