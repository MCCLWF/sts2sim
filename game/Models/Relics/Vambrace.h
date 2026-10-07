// Models/Relics/Vambrace.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct Vambrace : RelicModel {
    SIM_MODEL(Vambrace, RelicModel)

    void BeforeCombatStart() const {
        extra(0) = 0;
        extra(1) = 0;
        SetStatus(RelicStatus::Active);
    }

    Dec ModifyBlockMultiplicative(Creature, Dec, ValueProp props, CardModel cardSource, const CardPlay*) const {
        if (!IsCardOrMonsterMove(props) || !cardSource ||
            (extra(0) && static_cast<uint32_t>(extra(0)) != cardSource.Index()) ||
            cardSource.Owner() != Owner() || extra(1)) return Dec(1);
        return Dec(2);
    }

    void AfterModifyingBlockAmount(Dec modifiedAmount, CardModel cardSource, const CardPlay*) const {
        if (modifiedAmount <= Dec(0) || !cardSource) return;
        SetStatus(RelicStatus::Normal);
        extra(0) = cardSource.Index();
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !extra(0) ||
            play->Card.Index() != static_cast<uint32_t>(extra(0)) || extra(1)) return;
        extra(1) = 1;
    }

    void AfterCombatEnd(CombatRoom) const {
        extra(0) = 0;
        extra(1) = 0;
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
