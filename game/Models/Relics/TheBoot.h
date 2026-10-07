// Models/Relics/TheBoot.cs
#pragma once

#include "game/Models/RelicModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct TheBoot : RelicModel {
    SIM_MODEL(TheBoot, RelicModel)
    Dec ModifyHpLostAfterOstyLate(Creature target, Dec amount, ValueProp props, Creature dealer, CardModel) const {
        if (dealer != Owner().Creature() &&
            (!dealer || !dealer.IsMonster() || dealer.Monster().MonsterId_() != MonsterId::OSTY)) return amount;
        if (target == Owner().Creature() || !IsPoweredAttack(props) || amount < Dec(1) ||
            amount >= DynamicVars()["DamageMinimum"].BaseValue()) return amount;
        return DynamicVars()["DamageMinimum"].BaseValue();
    }
    void AfterModifyingHpLostAfterOsty() const {}
};

}  // namespace sts2
