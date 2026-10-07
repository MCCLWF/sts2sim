#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct JugglingPower : PowerModel {
    SIM_MODEL(JugglingPower, PowerModel)

    void AfterApplied(Creature, CardModel) const {
        extra(0) = Owner().CombatState().st().player_attack_cards_started_this_turn;
    }

    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner().Player() || play->Card.Type() != CardType::Attack) return;
        ++extra(0);
        if (extra(0) == 3) {
            for (int i = 0; i < Amount(); ++i)
                CardPileCmd::AddGeneratedCardToCombat(play->Card.CreateClone(), PileType::Hand, Owner().Player());
        }
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) extra(0) = 0;
    }
};

}  // namespace sts2
