// Models/Cards/HiddenDaggers.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Combat/CombatManager.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct HiddenDaggers : CardModel {
    SIM_MODEL(HiddenDaggers, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        const int32_t discardCount = DynamicVars().Cards().IntValue();
        CardList selected = CardSelectCmd::FromHandForDiscard(
            ctx, Owner(), CardSelectorPrefs(discardCount), nullptr, *this);
        CardCmd::Discard(ctx, selected);

        const int32_t shivCount = DynamicVars()["Shivs"].IntValue();
        if (shivCount == 0 || CombatManager(&sim()).IsOverOrEnding()) return;

        // Shiv.CreateInHand(owner, count, combatState) creates all cards
        // before adding them together, and returns those same card models.
        CardList shivs;
        sts2::CombatState combatState = CombatState();
        for (int32_t i = 0; i < shivCount; ++i) {
            shivs.push_back(combatState.CreateCard(CardId::SHIV, Owner()));
        }
        CardPileCmd::AddGeneratedCardsToCombat(shivs, PileType::Hand, Owner());

        if (CurrentUpgradeLevel() > 0) {
            for (CardModel shiv : shivs) CardCmd::Upgrade(shiv);
        }
    }
};

}  // namespace sts2
