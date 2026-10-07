// Models/Powers/SwipePower.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/PowerModel.h"
#include "game/Rewards/Reward.h"

namespace sts2 {
struct SwipePower : PowerModel {
    SIM_MODEL(SwipePower, PowerModel)

    CardModel StolenCard() const { return CardModel(sim_, static_cast<CardRef>(extra(0))); }
    void Steal(CardModel card) const {
        SetTarget(card.Owner().Creature());
        extra(0) = card.Index();
        if (card.DeckVersion()) CardPileCmd::RemoveFromDeck(card.DeckVersion(), false);
    }
    void BeforeDeath(Creature target) const {
        if (Owner() != target || !StolenCard() || !StolenCard().DeckVersion()) return;
        CardModel deckCard = StolenCard().DeckVersion();
        Target().Player().RunState().AddCard(deckCard, Target().Player());
        Reward reward = Reward::NewSpecialCardReward(Target().Player(), deckCard);
        sim_->g->run.combat_extra_rewards.push_back(reward.Index());
    }
};
}  // namespace sts2
