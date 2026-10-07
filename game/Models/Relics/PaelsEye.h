// Models/Relics/PaelsEye.cs (v0.111.0), single-player turn semantics.
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CardCmd.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PaelsEye : RelicModel {
    SIM_MODEL(PaelsEye, RelicModel)

    bool UsedThisCombat() const { return extra(0) != 0; }
    bool WasOwnerPartOfLastPlayerTurn() const { return extra(1) != 0; }
    bool AnyCardsPlayedThisTurn() const {
        PlayerCombatState combat = Owner().PlayerCombatState();
        if (!combat) return false;
        if (combat.TurnNumber() == 1 && Owner().GetRelic(RelicId::WHISPERING_EARRING)) return true;
        return sim_->g->combat.player_last_non_auto_card_play_turn == combat.TurnNumber();
    }

    void AfterObtained() const {
        CombatState combat = Owner().Creature().CombatState();
        extra(1) = CombatManager(sim_).IsInProgress() && combat &&
                   combat.CurrentSide() == CombatSide::Player;
    }
    void BeforeCardPlayed(const CardPlay* play) const {
        if (!CombatManager(sim_).IsInProgress() || UsedThisCombat() || !play ||
            play->IsAutoPlay || play->Card.Owner() != Owner()) return;
        SetStatus(RelicStatus::Normal);
    }
    void AfterSideTurnStart(CombatSide side, const CreatureList& participants, CombatState) const {
        if (side != Owner().Creature().Side() || UsedThisCombat()) return;
        const bool ownerParticipates = participants.contains(Owner().Creature());
        SetStatus(ownerParticipates ? RelicStatus::Active : RelicStatus::Normal);
        extra(1) = ownerParticipates;
    }
    bool ShouldTakeExtraTurn(Player player) const {
        return !UsedThisCombat() && !AnyCardsPlayedThisTurn() &&
               WasOwnerPartOfLastPlayerTurn() && player == Owner();
    }
    void BeforeSideTurnEndEarly(PlayerChoiceContext& context, CombatSide,
                                const CreatureList& participants) const {
        if (!participants.contains(Owner().Creature()) || UsedThisCombat() ||
            AnyCardsPlayedThisTurn() || !WasOwnerPartOfLastPlayerTurn()) return;
        CardList cards = Owner().PlayerCombatState().Hand().Cards();
        for (CardModel card : cards) CardCmd::Exhaust(context, card);
    }
    void AfterTakingExtraTurn(Player player) const {
        if (player != Owner()) return;
        Flash();
        SetStatus(RelicStatus::Normal);
        extra(0) = 1;
    }
    void AfterCombatEnd(CombatRoom) const {
        SetStatus(RelicStatus::Normal);
        extra(0) = 0;
    }
};

}  // namespace sts2
