// Models/Relics/JeweledMask.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct JeweledMask : RelicModel {
    SIM_MODEL(JeweledMask, RelicModel)

    void BeforeHandDraw(Player player, PlayerChoiceContext&, CombatState) const {
        if (player != Owner() || Owner().PlayerCombatState().TurnNumber() > 1) return;
        CardList powers;
        for (CardModel card : Owner().PlayerCombatState().DrawPile().Cards())
            if (card.Type() == CardType::Power) powers.push_back(card);
        if (powers.empty()) return;
        CardList nonInnate;
        for (CardModel card : powers)
            if (!card.Keywords().Contains(CardKeyword::Innate)) nonInnate.push_back(card);
        if (!nonInnate.empty()) powers = nonInnate;
        CardModel chosen = Owner().RunState().Rng().CombatCardSelection().NextItem(powers);
        Flash();
        chosen.SetToFreeThisTurn();
        CardPileCmd::Add(chosen, PileType::Hand);
    }
};

}  // namespace sts2
