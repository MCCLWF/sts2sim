// Models/Relics/ChoicesParadox.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct ChoicesParadox : RelicModel {
    SIM_MODEL(ChoicesParadox, RelicModel)

    void AfterPlayerTurnStart(PlayerChoiceContext& choiceContext, Player player) const {
        if (player != Owner() || Owner().PlayerCombatState().TurnNumber() != 1) return;
        Flash();
        CardList cards = CardFactory::GetForCombat(
            Owner(), sts2::data(Owner().Character()).card_pool, DynamicVars().Cards().IntValue(),
            Owner().RunState().Rng().CombatCardGeneration(), CardType::None, true);
        if (cards.empty()) return;
        for (CardModel card : cards) CardCmd::ApplyKeyword(card, {CardKeyword::Retain});
        for (CardModel card : CardSelectCmd::FromSimpleGrid(choiceContext, cards, Owner(), CardSelectorPrefs(1)))
            CardPileCmd::AddGeneratedCardToCombat(card, PileType::Hand, Owner());
    }
};

}  // namespace sts2
