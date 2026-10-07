// Models/Relics/IronClub.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct IronClub : RelicModel {
    SIM_MODEL(IronClub, RelicModel)

    int32_t CardsPlayed() const { return extra(0); }
    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return CardsPlayed() % DynamicVars().Cards().IntValue(); }

    void AfterCardPlayed(PlayerChoiceContext& context, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner()) return;
        int32_t played = ++extra(0);
        int32_t interval = DynamicVars().Cards().IntValue();
        // CardsPlayed setter -> UpdateDisplay.
        SetStatus(played % interval == interval - 1 ? RelicStatus::Active : RelicStatus::Normal);
        if (CombatManager(&sim()).IsInProgress() && played % interval == 0)
            CardPileCmd::Draw(context, Dec(1), Owner());
    }
};

}  // namespace sts2
