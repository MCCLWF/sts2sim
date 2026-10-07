// Models/Relics/Nunchaku.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Nunchaku : RelicModel {
    SIM_MODEL(Nunchaku, RelicModel)
    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return extra(0) % DynamicVars().Cards().IntValue(); }
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || play->Card.Type() != CardType::Attack) return;
        int32_t count = ++extra(0);
        int32_t perActivation = DynamicVars().Cards().IntValue();
        SetStatus(count % perActivation == perActivation - 1 ? RelicStatus::Active : RelicStatus::Normal);
        if (CombatManager(sim_).IsInProgress() && count % perActivation == 0)
            PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }
};

}  // namespace sts2
