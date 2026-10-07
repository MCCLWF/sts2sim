#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct KnifeTrap : CardModel {
    SIM_MODEL(KnifeTrap, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        CardList shivs;
        for (CardModel card : Owner().PlayerCombatState().ExhaustPile().Cards()) {
            if (card.HasTag(CardTag::Shiv)) shivs.push_back(card);
        }
        bool first = true;
        for (CardModel shiv : shivs) {
            if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(shiv);
            CardCmd::AutoPlay(ctx, shiv, play->Target, AutoPlayType::Default, false, !first);
            first = false;
        }
    }
};

}  // namespace sts2
