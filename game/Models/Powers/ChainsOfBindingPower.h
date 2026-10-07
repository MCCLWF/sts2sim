// Models/Powers/ChainsOfBindingPower.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/PowerModel.h"
#include "game/Models/AfflictionModel.h"

namespace sts2 {
struct ChainsOfBindingPower : PowerModel {
    SIM_MODEL(ChainsOfBindingPower, PowerModel)

    void AfterCardDrawn(PlayerChoiceContext&, CardModel card, bool) const {
        if (card.Owner() != Owner().Player() || CombatState().CurrentSide() != Owner().Side() ||
            !AfflictionModel::Pending(sim_, AfflictionId::BOUND).CanAfflict(card)) return;
        if (sim_->g->combat.player_bound_cards_afflicted_this_turn < Amount()) {
            CardCmd::AfflictAndPreview(AfflictionId::BOUND, CardList{card}, Dec(Amount()));
        }
    }
    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.IsDupe() || play->Card.Owner().Creature() != Owner()) return;
        if (play->Card.Affliction() && play->Card.Affliction().AfflictionId_() == AfflictionId::BOUND) extra(0) = 1;
    }
    bool ShouldPlay(CardModel card, AutoPlayType) const {
        if (card.Owner().Creature() != Owner() || !card.Affliction() || card.Affliction().AfflictionId_() != AfflictionId::BOUND) return true;
        return extra(0) == 0;
    }
    void BeforeSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        extra(0) = 0;
        Player player = Owner().Player();
        if (!player || !player.PlayerCombatState()) return;
        for (auto card : player.PlayerCombatState().AllCards()) {
            if (card.Affliction() && card.Affliction().AfflictionId_() == AfflictionId::BOUND) CardCmd::ClearAffliction(card);
        }
    }
};
}  // namespace sts2
