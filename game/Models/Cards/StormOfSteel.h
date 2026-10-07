#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct StormOfSteel : CardModel {
    SIM_MODEL(StormOfSteel, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList discarded = Owner().PlayerCombatState().Hand().Cards();
        const int32_t handSize = static_cast<int32_t>(discarded.size());
        CardCmd::Discard(ctx, discarded);
        for (int32_t i = 0; i < handSize; ++i) {
            CardModel shiv = CombatState().CreateCard(CardId::SHIV, Owner());
            CardPileCmd::AddGeneratedCardToCombat(shiv, PileType::Hand, Owner());
            if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(shiv);
        }
    }
};

}  // namespace sts2
