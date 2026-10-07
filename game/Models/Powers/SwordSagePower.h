#pragma once

#include "game/Models/PowerModel.h"

namespace sts2 {
struct SwordSagePower : PowerModel {
    SIM_MODEL(SwordSagePower, PowerModel)
    void AfterPowerAmountChanged(PlayerChoiceContext&, PowerModel power, Dec amount, Creature, CardModel) const {
        if (power.PowerId_() != PowerId::SWORD_SAGE_POWER || power.Owner() != Owner()) return;
        for (CardModel card : Owner().Player().PlayerCombatState().AllCards())
            if (card.CardId_() == CardId::SOVEREIGN_BLADE) card.SetBaseReplayCount(card.BaseReplayCount() + amount.to_int());
    }
    void AfterCardEnteredCombat(CardModel card) const {
        if (!card || card.CloneOf() || card.Owner() != Owner().Player() || card.CardId_() != CardId::SOVEREIGN_BLADE) return;
        card.SetBaseReplayCount(card.BaseReplayCount() + Amount());
    }
    void AfterRemoved(Creature oldOwner) const {
        Player player = oldOwner.Player();
        if (!player) return;
        for (CardModel card : player.PlayerCombatState().AllCards())
            if (card.CardId_() == CardId::SOVEREIGN_BLADE) card.SetBaseReplayCount(card.BaseReplayCount() - Amount());
    }
};
}  // namespace sts2
