#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct HelloWorldPower : PowerModel {
    SIM_MODEL(HelloWorldPower, PowerModel)

    void BeforeHandDraw(Player player, PlayerChoiceContext&, sts2::CombatState) const {
        if (player != Owner().Player() || AmountOnTurnStart() < 1) return;
        CardList cards = CardFactory::GetForCombat(player, sts2::data(player.Character()).card_pool,
                                                   AmountOnTurnStart(), player.RunState().Rng().CombatCardGeneration(),
                                                   CardType::None, true, CardId::NONE, CardRarity::Common);
        CardPileCmd::AddGeneratedCardsToCombat(cards, PileType::Hand, player);
    }
};

}  // namespace sts2
