// Models/Events/BrainLeech.cs (IsAllowed partial port)
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/EventModel.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct BrainLeech : EventModel {
    SIM_MODEL(BrainLeech, EventModel)
    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(BrainLeech, ShareKnowledge), InitialOptionKey("SHARE_KNOWLEDGE")));
        list.push_back(EventOption(SIM_OPTION(BrainLeech, Rip), InitialOptionKey("RIP"))
                           .ThatDoesDamage(DynamicVars()["RipHpLoss"].BaseValue()));
        return list;
    }

    void Rip(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars()["RipHpLoss"], Creature{});
        CardCreationOptionsState options = CardCreationOptions::ForNonCombatWithDefaultOdds({CardPoolId::COLORLESS_CARD_POOL});
        options.flags = options.flags | CardCreationFlags::NoRarityModification |
                        CardCreationFlags::NoCardPoolModifications;
        for (int32_t i = 0; i < DynamicVars()["RewardCount"].IntValue(); ++i) {
            StaticVec<Reward, 8> rewards;
            rewards.push_back(Reward::NewCardReward(Owner(), options, 3));
            RewardsCmd::OfferCustom(Owner(), rewards);
        }
        SetEventFinished(L10NLookup("BRAIN_LEECH.pages.RIP.description"));
    }

    void ShareKnowledge(const EventOptionState&) const {
        CardCreationOptionsState options = CardCreationOptions::ForNonCombatWithDefaultOdds({sts2::data(Owner().Character()).card_pool});
        CardList cards = CardFactory::CreateForReward(Owner(), DynamicVars()["FromCardChoiceCount"].IntValue(), options);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CardList selected = CardSelectCmd::FromSimpleGridForRewards(ctx, cards, Owner(), CardSelectorPrefs(1));
        for (CardModel card : selected) CardPileCmd::Add(card, PileType::Deck);
        SetEventFinished(L10NLookup("BRAIN_LEECH.pages.SHARE_KNOWLEDGE.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return runState.CurrentActIndex() < 2; }
};

}  // namespace sts2
