#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct SpectrumShiftPower : PowerModel {
    SIM_MODEL(SpectrumShiftPower, PowerModel)
    void BeforeHandDraw(Player player, PlayerChoiceContext&, sts2::CombatState) const {
        if (player != Owner().Player()) return;
        CardList generated = CardFactory::GetForCombat(player, CardPoolId::COLORLESS_CARD_POOL, Amount(),
            player.RunState().Rng().CombatCardGeneration(), CardType::None, true);
        CardPileCmd::AddGeneratedCardsToCombat(generated, PileType::Hand, player);
    }
};
}  // namespace sts2
