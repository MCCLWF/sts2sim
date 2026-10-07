// Models/Relics/Toolbox.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Toolbox : RelicModel {
    SIM_MODEL(Toolbox, RelicModel)

    void BeforeHandDraw(Player player, PlayerChoiceContext& choiceContext, CombatState) const {
        if (player != Owner() || Owner().PlayerCombatState().TurnNumber() != 1) return;
        CardList cards = CardFactory::GetForCombat(
            Owner(), CardPoolId::COLORLESS_CARD_POOL, DynamicVars().Cards().IntValue(),
            Owner().RunState().Rng().CombatCardGeneration(), CardType::None, true);
        CardModel chosen = CardSelectCmd::FromChooseACardScreen(choiceContext, cards, Owner());
        if (chosen) CardPileCmd::AddGeneratedCardToCombat(chosen, PileType::Hand, player);
    }
};

}  // namespace sts2
