#pragma once

#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct TrackingPower : PowerModel {
    SIM_MODEL(TrackingPower, PowerModel)

    Dec ModifyDamageMultiplicative(Creature target, Dec, ValueProp props, Creature dealer, CardModel card,
                                   const CardPlay*) const {
        if (!IsPoweredAttack(props) || !card || (!dealer || (dealer != Owner() &&
                !(dealer.IsPet() && dealer.PetOwner() == Owner().Player()))) ||
            !target || !target.HasPower(PowerId::WEAK_POWER)) return Dec(1);
        return Dec(1) + Dec(Amount()) / Dec(100);
    }
};

}  // namespace sts2
