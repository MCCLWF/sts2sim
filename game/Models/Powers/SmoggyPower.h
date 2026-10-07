// Models/Powers/SmoggyPower.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct SmoggyPower : PowerModel {
    SIM_MODEL(SmoggyPower, PowerModel)

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* cardPlay) const {
        if (cardPlay == nullptr || cardPlay->Card.Owner().Creature() != Owner() || cardPlay->Card.Type() != CardType::Skill) return;
        Flash();
        Player player = Owner().Player();
        if (!player || !player.PlayerCombatState()) return;
        for (CardModel card : player.PlayerCombatState().AllCards()) {
            if (card.Type() == CardType::Skill && !card.Affliction()) {
                CardCmd::Afflict(AfflictionId::SMOG, card, Dec(1));
            }
        }
    }

    void AfterCardEnteredCombat(CardModel card) const {
        if (card.Owner() == Owner().Player() && !card.Affliction() && card.Type() == CardType::Skill &&
            Owner().CombatState().st().player_played_skill_this_turn) {
            CardCmd::Afflict(AfflictionId::SMOG, card, Dec(1));
        }
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        Player player = Owner().Player();
        if (!player || !player.PlayerCombatState()) return;
        for (CardModel card : player.PlayerCombatState().AllCards()) {
            if (card.Affliction() && card.Affliction().AfflictionId_() == AfflictionId::SMOG) {
                CardCmd::ClearAffliction(card);
            }
        }
    }

    bool ShouldPlay(CardModel card, AutoPlayType) const {
        if (card.Owner() != Owner().Player()) return true;
        return !card.Affliction() || card.Affliction().AfflictionId_() != AfflictionId::SMOG;
    }
};

}  // namespace sts2
