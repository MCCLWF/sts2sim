// Models/Relics/RippleBasin.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct RippleBasin : RelicModel {
    SIM_MODEL(RippleBasin, RelicModel)

    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature())) return;
        extra(0) = 0;
        SetStatus(RelicStatus::Active);
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Type() != CardType::Attack || play->Card.Owner() != Owner()) return;
        extra(0) = 1;
        SetStatus(RelicStatus::Normal);
    }

    void BeforeSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner().Creature()) || extra(0)) return;
        SetStatus(RelicStatus::Normal);
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), nullptr);
    }

    void AfterCombatEnd(CombatRoom) const {
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
