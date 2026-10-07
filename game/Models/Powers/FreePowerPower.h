// Models/Powers/FreePowerPower.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct FreePowerPower : PowerModel { SIM_MODEL(FreePowerPower, PowerModel)
    bool TryModifyEnergyCostInCombatLate(CardModel card, Dec original, Dec& modified) const {
        modified = original;
        if (card.Owner().Creature() != Owner() || card.Type() != CardType::Power) return false;
        CardPile pile = card.Pile();
        if (!pile || (pile.Type() != PileType::Hand && pile.Type() != PileType::Play)) return false;
        modified = Dec(0);
        return true;
    }
    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.Owner().Creature() != Owner() || play->Card.Type() != CardType::Power) return;
        CardPile pile = play->Card.Pile();
        if (pile && (pile.Type() == PileType::Hand || pile.Type() == PileType::Play)) PowerCmd::Decrement(*this);
    }
};
}
