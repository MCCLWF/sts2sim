#pragma once

#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct AccuracyPower : PowerModel {
    SIM_MODEL(AccuracyPower, PowerModel)

    Dec ModifyDamageAdditive(Creature, Dec, ValueProp props, Creature dealer, CardModel card, const CardPlay*) const {
        if (Owner() != dealer || !IsPoweredAttack(props) || !card || !card.HasTag(CardTag::Shiv)) return Dec(0);
        return Dec(Amount());
    }
};

}  // namespace sts2
