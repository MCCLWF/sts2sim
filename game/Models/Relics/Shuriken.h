// Models/Relics/Shuriken.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Shuriken : RelicModel {
    SIM_MODEL(Shuriken, RelicModel)
    bool ShowCounter() const { return CombatManager(sim_).IsInProgress(); }
    int32_t DisplayAmount() const { return extra(0) % DynamicVars().Cards().IntValue(); }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) { extra(0) = 0; SetStatus(RelicStatus::Normal); }
    }
    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress() ||
            play->Card.Type() != CardType::Attack) return;
        int32_t count = ++extra(0);
        int32_t perActivation = DynamicVars().Cards().IntValue();
        if (count % perActivation == 0)
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(), DynamicVars().Strength().BaseValue(),
                            Owner().Creature(), CardModel{});
        SetStatus(count % perActivation == perActivation - 1 ? RelicStatus::Active : RelicStatus::Normal);
    }
    void AfterCombatEnd(CombatRoom) const { extra(0) = 0; SetStatus(RelicStatus::Normal); }
};

}  // namespace sts2
