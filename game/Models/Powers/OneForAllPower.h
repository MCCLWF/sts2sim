#pragma once
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct OneForAllPower : PowerModel { SIM_MODEL(OneForAllPower, PowerModel)
    Dec ModifyDamageAdditive(Creature, Dec, ValueProp props, Creature, CardModel cardSource, const CardPlay* play) const {
        if (!IsPoweredAttack(props) || !cardSource || cardSource.Owner().Creature() != Owner()) return Dec(0);
        bool costsX = play ? play->Card.EnergyCost().CostsX() : cardSource.EnergyCost().CostsX();
        if (costsX || (play && play->Resources.EnergySpent != 0)) return Dec(0);
        if (!play && cardSource.EnergyCost().GetWithModifiers(CostModifiers::All) != 0) return Dec(0);
        return Dec(Amount());
    }
};
}
