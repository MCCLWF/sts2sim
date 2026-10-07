// Models/Cards/BladeDance.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct BladeDance : CardModel {
    SIM_MODEL(BladeDance, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i)
            CardPileCmd::AddGeneratedCardToCombat(Owner().Creature().CombatState().CreateCard(CardId::SHIV, Owner()), PileType::Hand, Owner());
    }
};
}
