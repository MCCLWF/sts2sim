// Models/Relics/BlessedAntler.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BlessedAntler : RelicModel {
    SIM_MODEL(BlessedAntler, RelicModel)

    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() ? amount + DynamicVars().Energy().BaseValue() : amount;
    }

    void BeforeHandDraw(Player player, PlayerChoiceContext&, CombatState combatState) const {
        if (player != Owner() || Owner().PlayerCombatState().TurnNumber() != 1) return;
        CardList dazed;
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i)
            dazed.push_back(combatState.CreateCard(CardId::DAZED, Owner()));
        CardPileCmd::AddGeneratedCardsToCombat(dazed, PileType::Draw, Owner(), CardPilePosition::Random);
    }
};

}  // namespace sts2
