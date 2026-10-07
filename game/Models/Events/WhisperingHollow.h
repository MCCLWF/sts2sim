// Models/Events/WhisperingHollow.cs (IsAllowed partial port)
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct WhisperingHollow : EventModel {
    SIM_MODEL(WhisperingHollow, EventModel)
    void CalculateVars() const { DynamicVars().Gold().SetBaseValue(DynamicVars().Gold().BaseValue() + Dec(Rng().NextInt(-9, 10))); }
    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(WhisperingHollow, Gold), InitialOptionKey("GOLD")));
        list.push_back(EventOption(SIM_OPTION(WhisperingHollow, Hug), InitialOptionKey("HUG"))
                           .ThatDoesDamage(DynamicVars().HpLoss().BaseValue()));
        return list;
    }
    void Gold(const EventOptionState&) const {
        PlayerCmd::LoseGold(DynamicVars().Gold().BaseValue(), Owner(), GoldLossType::Spent);
        StaticVec<Reward, 8> rewards;
        rewards.push_back(Reward::NewPotionReward(Owner()));
        rewards.push_back(Reward::NewPotionReward(Owner()));
        RewardsCmd::OfferCustom(Owner(), rewards);
        SetEventFinished(L10NLookup("WHISPERING_HOLLOW.pages.GOLD.description"));
    }
    void Hug(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForTransformation(Owner(), CardSelectorPrefs(1));
        if (!cards.empty()) CardCmd::TransformToRandom(cards[0], Rng());
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().HpLoss(), Creature{});
        SetEventFinished(L10NLookup("WHISPERING_HOLLOW.pages.HUG.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return all_players(runState, [](Player p) { return p.Gold() >= 44; }); }
};

}  // namespace sts2
