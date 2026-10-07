#pragma once

#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct FastenPower : PowerModel {
    SIM_MODEL(FastenPower, PowerModel)

    Dec ModifyBlockAdditive(Creature target, Dec, ValueProp props, CardModel cardSource,
                            const CardPlay*) const {
        if (target != Owner() || !IsPoweredCardOrMonsterMoveBlock(props)) return Dec(0);
        if (cardSource && !cardSource.HasTag(CardTag::Defend)) return Dec(0);
        return Dec(Amount());
    }

    void AfterModifyingBlockAmount(Dec, CardModel, const CardPlay*) const { Flash(); }
};

}  // namespace sts2
