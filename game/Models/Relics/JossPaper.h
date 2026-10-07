// Models/Relics/JossPaper.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct JossPaper : RelicModel {
    SIM_MODEL(JossPaper, RelicModel)
    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return extra(0); }

    void SetCardsExhausted(int32_t count) const {
        extra(0) = count;
        SetStatus(count == DynamicVars()["ExhaustAmount"].IntValue() - 1 ? RelicStatus::Active : RelicStatus::Normal);
    }
    void DrawIfThresholdMet(PlayerChoiceContext& ctx) const {
        int32_t threshold = DynamicVars()["ExhaustAmount"].IntValue();
        if (extra(0) < threshold) return;
        int32_t draws = extra(0) / threshold;
        CardPileCmd::Draw(ctx, Dec(draws), Owner());
        SetCardsExhausted(extra(0) % threshold);
    }
    void AfterCardExhausted(PlayerChoiceContext& ctx, CardModel card, bool causedByEthereal) const {
        if (card.Owner() != Owner()) return;
        if (causedByEthereal) { ++extra(1); return; }
        SetCardsExhausted(extra(0) + 1);
        DrawIfThresholdMet(ctx);
    }
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner().Creature())) return;
        SetCardsExhausted(extra(0) + extra(1));
        extra(1) = 0;
        DrawIfThresholdMet(ctx);
    }
    void AfterCombatEnd(CombatRoom) const { extra(1) = 0; }
};

}  // namespace sts2
