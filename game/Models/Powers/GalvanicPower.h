#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct GalvanicPower : PowerModel {
    SIM_MODEL(GalvanicPower, PowerModel)

    void AfflictPowerCards(CardList cards) const {
        for (CardModel card : cards) {
            if (card.Type() == CardType::Power && card.Affliction().IsNull()) {
                CardCmd::Afflict(AfflictionId::GALVANIZED, card, Dec(Amount()));
            }
        }
    }

    void BeforeCombatStart() const {
        for (Player player : Owner().CombatState().Players()) {
            AfflictPowerCards(player.PlayerCombatState().AllCards());
        }
    }

    void AfterCardEnteredCombat(CardModel card) const {
        if (card.Type() == CardType::Power && !card.Affliction()) {
            CardCmd::Afflict(AfflictionId::GALVANIZED, card, Dec(Amount()));
        }
    }

    void AfterCardPlayed(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (cardPlay == nullptr) return;
        AfflictionModel affliction = cardPlay->Card.Affliction();
        if (!affliction || affliction.AfflictionId_() != AfflictionId::GALVANIZED) return;
        CreatureCmd::Damage(choiceContext, cardPlay->Card.Owner().Creature(), Dec(Amount()),
                            ValueProp::Unpowered | ValueProp::Move, Creature{}, CardModel{}, nullptr);
    }
};

}  // namespace sts2
