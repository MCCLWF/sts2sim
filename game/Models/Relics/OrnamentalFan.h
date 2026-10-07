// Models/Relics/OrnamentalFan.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct OrnamentalFan : RelicModel {
    SIM_MODEL(OrnamentalFan, RelicModel)
    bool ShowCounter() const { return CombatManager(sim_).IsInProgress(); }
    int32_t DisplayAmount() const { return extra(0) % DynamicVars().Cards().IntValue(); }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) { extra(0) = 0; SetStatus(RelicStatus::Normal); }
    }
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress() ||
            play->Card.Type() != CardType::Attack) return;
        int32_t count = ++extra(0);
        int32_t perActivation = DynamicVars().Cards().IntValue();
        if (count % perActivation == 0)
            CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
        SetStatus(count % perActivation == perActivation - 1 ? RelicStatus::Active : RelicStatus::Normal);
    }
    void AfterCombatEnd(CombatRoom) const { SetStatus(RelicStatus::Normal); }
};

}  // namespace sts2
