// Models/Relics/TuningFork.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct TuningFork : RelicModel {
    SIM_MODEL(TuningFork, RelicModel)

    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return extra(0); }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || play->Card.Type() != CardType::Skill) return;
        ++extra(0);
        if (extra(0) >= DynamicVars().Cards().IntValue()) {
            SetStatus(RelicStatus::Normal);
            CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
            extra(0) -= DynamicVars().Cards().IntValue();
        } else {
            SetStatus(extra(0) == DynamicVars().Cards().IntValue() - 1 ? RelicStatus::Active : RelicStatus::Normal);
        }
    }
};

}  // namespace sts2
