#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct HiddenGem : CardModel {
    SIM_MODEL(HiddenGem, CardModel)

    bool CanBeGeneratedInCombat() const { return false; }

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        CardList preferred;
        CardList eligible;
        for (CardModel card : Owner().PlayerCombatState().DrawPile().Cards()) {
            if (card.Keywords().Contains(CardKeyword::Unplayable) || card.Type() == CardType::Curse ||
                card.Type() == CardType::Quest || card.GetEnchantedReplayCount() >= 1) continue;
            eligible.push_back(card);
            if (card.Type() == CardType::Attack || card.Type() == CardType::Skill || card.Type() == CardType::Power)
                preferred.push_back(card);
        }
        CardModel chosen = Owner().RunState().Rng().CombatCardSelection().NextItem(preferred.empty() ? eligible : preferred);
        if (chosen) chosen.SetBaseReplayCount(chosen.BaseReplayCount() + DynamicVars()["Replay"].IntValue());
    }

};

}  // namespace sts2
