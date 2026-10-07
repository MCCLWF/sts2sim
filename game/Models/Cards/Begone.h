#pragma once

#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Begone : CardModel {
    SIM_MODEL(Begone, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList selection = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(1), nullptr, *this);
        if (selection.empty()) return;
        CardModel replacement = CombatState().CreateCard(CardId::MINION_STRIKE, Owner());
        if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(replacement);
        CardCmd::Transform(selection[0], replacement);
    }
};
}  // namespace sts2
