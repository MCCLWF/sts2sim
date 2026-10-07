// Models/Cards/SummonForth.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/ForgeCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct SummonForth : CardModel {
    SIM_MODEL(SummonForth, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        CardList blades;
        for (CardModel card : Owner().PlayerCombatState().AllCards()) {
            if (card.CardId_() == CardId::SOVEREIGN_BLADE && card.Pile().Type() != PileType::Hand) blades.push_back(card);
        }
        if (!blades.empty()) CardPileCmd::Add(blades, PileType::Hand);
        ForgeCmd::Forge(DynamicVars().Forge().BaseValue(), Owner(), *this);
    }
};
}  // namespace sts2
