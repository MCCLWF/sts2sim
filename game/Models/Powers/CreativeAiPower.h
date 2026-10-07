#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct CreativeAiPower : PowerModel { SIM_MODEL(CreativeAiPower, PowerModel)
    void BeforeHandDraw(Player player, PlayerChoiceContext& ctx, sts2::CombatState) const {
        if (player != Owner().Player()) return;
        for (int32_t i = 0; i < Amount(); ++i) {
            CardList cards = CardFactory::GetForCombat(player, sts2::data(player.Character()).card_pool, 1,
                player.RunState().Rng().CombatCardGeneration(), CardType::Power, true);
            if (!cards.empty()) CardPileCmd::AddGeneratedCardToCombat(cards[0], PileType::Hand, player);
        }
    }
};
}
