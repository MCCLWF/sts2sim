// Models/Powers/DexterityPower.cs
#pragma once
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct DexterityPower : PowerModel {
    SIM_MODEL(DexterityPower, PowerModel)
    bool AllowNegative() const { return true; }
    Dec ModifyBlockAdditive(Creature target, Dec, ValueProp props, CardModel cardSource, const CardPlay*) const {
        if (cardSource ? cardSource.Owner().Creature() != Owner() : Owner() != target) return Dec(0);
        return IsPoweredCardOrMonsterMoveBlock(props) ? Dec(Amount()) : Dec(0);
    }
};
}
