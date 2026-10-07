// Models/Relics/RazorTooth.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct RazorTooth : RelicModel {
    SIM_MODEL(RazorTooth, RelicModel)

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* cardPlay) const {
        if (!cardPlay) return;
        CardModel card = cardPlay->Card;
        if (card.Owner() != Owner()) return;
        if (card.Type() != CardType::Attack && card.Type() != CardType::Skill) return;
        if (card.IsUpgradable()) CardCmd::Upgrade(card);
    }
};

}  // namespace sts2
