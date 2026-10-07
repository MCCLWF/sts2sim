#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct BundleOfJoy : CardModel {
    SIM_MODEL(BundleOfJoy, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        CardList generated = CardFactory::GetForCombat(Owner(), CardPoolId::COLORLESS_CARD_POOL, DynamicVars().Cards().IntValue(), Owner().RunState().Rng().CombatCardGeneration(), CardType::None, true);
        CardPileCmd::AddGeneratedCardsToCombat(generated, PileType::Hand, Owner());
    }
};
}  // namespace sts2
