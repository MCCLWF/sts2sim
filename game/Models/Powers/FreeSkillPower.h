#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct FreeSkillPower : PowerModel {
    SIM_MODEL(FreeSkillPower, PowerModel)

    bool TryModifyEnergyCostInCombatLate(CardModel card, Dec originalCost, Dec& modifiedCost) const {
        modifiedCost = originalCost;
        if (!card || card.Owner().Creature() != Owner() || card.Type() != CardType::Skill) return false;
        CardPile pile = card.Pile();
        if (!pile || (pile.Type() != PileType::Hand && pile.Type() != PileType::Play)) return false;
        modifiedCost = Dec(0);
        return true;
    }

    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.Owner().Creature() != Owner() || play->Card.Type() != CardType::Skill) return;
        CardPile pile = play->Card.Pile();
        if (pile && (pile.Type() == PileType::Hand || pile.Type() == PileType::Play)) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
