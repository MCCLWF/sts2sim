// Models/Relics/VexingPuzzlebox.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct VexingPuzzlebox : RelicModel {
    SIM_MODEL(VexingPuzzlebox, RelicModel)
    void AfterPlayerTurnStart(PlayerChoiceContext&, Player player) const {
        if (player != Owner() || Owner().PlayerCombatState().TurnNumber() != 1) return;
        CardPoolId pool = sts2::data(Owner().Character()).card_pool;
        CardList cards = CardFactory::GetForCombat(Owner(), pool, 1,
                                                   Owner().RunState().Rng().CombatCardGeneration(), CardType::None, true);
        if (cards.empty()) return;
        cards[0].SetToFreeThisTurn();
        CardPileCmd::AddGeneratedCardToCombat(cards[0], PileType::Hand, player);
    }
};

}  // namespace sts2
