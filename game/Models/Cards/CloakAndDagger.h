// Models/Cards/CloakAndDagger.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct CloakAndDagger : CardModel {
    SIM_MODEL(CloakAndDagger, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i)
            CardPileCmd::AddGeneratedCardToCombat(Owner().Creature().CombatState().CreateCard(CardId::SHIV, Owner()), PileType::Hand, Owner());
    }
};
}
