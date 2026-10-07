#pragma once

#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Charge : CardModel {
    SIM_MODEL(Charge, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList selection = CardSelectCmd::FromCombatPile(ctx, CardPile::Get(PileType::Draw, Owner()), Owner(), CardSelectorPrefs(DynamicVars().Cards().IntValue()));
        for (CardModel card : selection) {
            auto result = CardCmd::Transform(card, CombatState().CreateCard(CardId::MINION_DIVE_BOMB, Owner()));
            if (CurrentUpgradeLevel() > 0 && result.has_value() && result->success) CardCmd::Upgrade(result->cardAdded);
        }
    }
};
}  // namespace sts2
