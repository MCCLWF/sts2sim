// Models/Cards/Enthralled.cs. Required by the in-scope Blood-Soaked Rose.
#pragma once

#include "game/Models/CardModel.h"

namespace sts2 {

struct Enthralled : CardModel {
    SIM_MODEL(Enthralled, CardModel)

    bool CanBeGeneratedByModifiers() const { return false; }
    int32_t MaxUpgradeLevel() const { return 0; }

    bool ShouldPlay(CardModel card, AutoPlayType autoPlayType) const {
        if (card.Owner() != Owner()) return true;
        CardPile pile = Pile();
        if (!pile || pile.Type() != PileType::Hand || card.CardId_() == CardId::ENTHRALLED ||
            autoPlayType != AutoPlayType::None) return true;
        return false;
    }
};

}  // namespace sts2
