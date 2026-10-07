#pragma once

#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Guards : CardModel {
    SIM_MODEL(Guards, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList selection = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(0, 999999999), nullptr, *this);
        for (CardModel selected : selection) {
            CardModel replacement = CombatState().CreateCard(CardId::MINION_SACRIFICE, Owner());
            if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(replacement);
            CardCmd::Transform(selected, replacement);
        }
    }
};
}  // namespace sts2
