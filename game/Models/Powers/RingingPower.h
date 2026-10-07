#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Entities/Players/PlayerCombatState.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct RingingPower : PowerModel {
    SIM_MODEL(RingingPower, PowerModel)

    void AfterApplied(Creature, CardModel) const {
        Player player = Owner().Player();
        if (!player || !player.PlayerCombatState()) return;
        for (CardModel card : player.PlayerCombatState().AllCards()) Afflict(card);
    }

    void AfterCardEnteredCombat(CardModel card) const {
        if (card.Owner() == Owner().Player()) Afflict(card);
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Remove(*this);
    }

    void AfterRemoved(Creature oldOwner) const {
        Player player = oldOwner.Player();
        if (!player || !player.PlayerCombatState()) return;
        for (CardModel card : player.PlayerCombatState().AllCards()) {
            if (card.Affliction() && card.Affliction().AfflictionId_() == AfflictionId::RINGING)
                CardCmd::ClearAffliction(card);
        }
    }

    bool ShouldPlay(CardModel card, AutoPlayType) const {
        if (card.Owner() != Owner().Player() || !card.Affliction() ||
            card.Affliction().AfflictionId_() != AfflictionId::RINGING)
            return true;
        // Source checks CardPlaysStarted, including an outer card whose OnPlay
        // has not finished yet. Nested AutoPlay must already be blocked.
        return sim().g->combat.player_all_cards_started_this_turn == 0;
    }

private:
    void Afflict(CardModel card) const {
        if (!card.Affliction()) CardCmd::Afflict(AfflictionId::RINGING, card, Dec(1));
    }
};

}  // namespace sts2
