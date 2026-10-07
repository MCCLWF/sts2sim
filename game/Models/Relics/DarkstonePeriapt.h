// Models/Relics/DarkstonePeriapt.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct DarkstonePeriapt : RelicModel {
    SIM_MODEL(DarkstonePeriapt, RelicModel)

    void AfterCardChangedPiles(CardModel card, PileType, AbstractModel) const {
        CardPile pile = card.Pile();
        if (pile && pile.Type() == PileType::Deck && card.Owner() == Owner() && card.Type() == CardType::Curse) {
            Flash();
            CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue());
        }
    }
};

}  // namespace sts2
