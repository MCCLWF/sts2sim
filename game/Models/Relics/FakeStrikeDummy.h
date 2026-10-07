// Models/Relics/FakeStrikeDummy.cs (v0.111.0).
#pragma once
#include "game/Models/RelicModel.h"
namespace sts2 {
struct FakeStrikeDummy : RelicModel {
    SIM_MODEL(FakeStrikeDummy, RelicModel)
    int32_t MerchantCost() const { return 50; }
    Dec ModifyDamageAdditive(Creature, Dec, ValueProp props, Creature dealer, CardModel cardSource,
                             const CardPlay*) const {
        if (!IsPoweredAttack(props) || !cardSource || !cardSource.HasTag(CardTag::Strike)) return Dec(0);
        if (dealer != Owner().Creature() && cardSource.Owner() != Owner()) return Dec(0);
        return DynamicVars()["ExtraDamage"].BaseValue();
    }
};
}
