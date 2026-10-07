#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Quasar : CardModel {
    SIM_MODEL(Quasar, CardModel)
    int32_t CanonicalStarCost() const { return 2; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList generated = CardFactory::GetForCombat(Owner(), CardPoolId::COLORLESS_CARD_POOL, 3, Owner().RunState().Rng().CombatCardGeneration(), CardType::None, true);
        if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(generated);
        CardModel choice = CardSelectCmd::FromChooseACardScreen(ctx, generated, Owner(), true);
        if (choice) CardPileCmd::AddGeneratedCardToCombat(choice, PileType::Hand, Owner());
    }
};
}  // namespace sts2
