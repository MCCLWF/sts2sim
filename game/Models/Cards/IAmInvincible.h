// Models/Cards/IAmInvincible.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct IAmInvincible : CardModel {
    SIM_MODEL(IAmInvincible, CardModel)

    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* cardPlay) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), cardPlay);
    }
    void AfterAutoPostPlayPhaseEntered(PlayerChoiceContext& choiceContext, Player player) const {
        if (player != Owner()) return;
        CardList cards = CardPile::Get(PileType::Draw, Owner()).Cards();
        if (!cards.empty() && cards.front() == *this) {
            CardPileCmd::AutoPlayFromDrawPile(choiceContext, Owner(), 1, CardPilePosition::Top, false);
        }
    }
};

}  // namespace sts2
