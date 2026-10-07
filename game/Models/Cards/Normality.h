// Models/Cards/Normality.cs
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct Normality : CardModel {
    SIM_MODEL(Normality, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    bool ShouldPlay(CardModel card, AutoPlayType) const {
        if (card.Owner() != Owner()) return true;
        CardPile pile = Pile();
        if (!pile || pile.Type() != PileType::Hand) return true;
        return sim().g->combat.player_all_cards_started_this_turn < 3;
    }
};

}  // namespace sts2
