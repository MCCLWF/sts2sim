// Models/Events/TheLegendsWereTrue.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct TheLegendsWereTrue : EventModel {
    SIM_MODEL(TheLegendsWereTrue, EventModel)

    bool IsAllowed(RunState runState) const {
        if (runState.CurrentActIndex() != 0) return false;
        for (Player p : runState.Players()) {
            if (p.Deck().IsEmpty() || p.Creature().CurrentHp() < 10) return false;
        }
        return true;
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(TheLegendsWereTrue, NabTheMap),
                                   "THE_LEGENDS_WERE_TRUE.pages.INITIAL.options.NAB_THE_MAP"));
        list.push_back(EventOption(SIM_OPTION(TheLegendsWereTrue, SlowlyFindAnExit),
                                   "THE_LEGENDS_WERE_TRUE.pages.INITIAL.options.SLOWLY_FIND_AN_EXIT")
                           .ThatDoesDamage(DynamicVars().Damage().BaseValue()));
        return list;
    }

    void NabTheMap(const EventOptionState&) const {
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::SPOILS_MAP, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("THE_LEGENDS_WERE_TRUE.pages.NAB_THE_MAP.description"));
    }

    void SlowlyFindAnExit(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().Damage(), Creature{});
        StaticVec<PotionId, 96> potion_options = PotionFactory::GetPotionOptions(Owner());
        PotionId potion_id = Owner().PlayerRng().Rewards().NextItem(potion_options);
        if (potion_id != PotionId::NONE) {
            StaticVec<Reward, 8> rewards;
            PotionModel potion(&sim(), alloc_potion(*sim_->g, potion_id, 0));
            rewards.push_back(Reward::NewPotionReward(Owner(), potion));
            RewardsCmd::OfferCustom(Owner(), rewards);
        }
        SetEventFinished(L10NLookup("THE_LEGENDS_WERE_TRUE.pages.SLOWLY_FIND_AN_EXIT.description"));
    }
};

}  // namespace sts2
