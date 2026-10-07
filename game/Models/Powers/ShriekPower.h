// Models/Powers/ShriekPower.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct ShriekPower : PowerModel {
    SIM_MODEL(ShriekPower, PowerModel)
    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result, ValueProp, Creature, CardModel) const {
        if (target == Owner() && result.UnblockedDamage > 0 && target.CurrentHp() <= Amount()) {
            CreatureCmd::Stun(Owner(), "TERROR_MOVE");
            PowerCmd::Remove(*this);
        }
    }
};
}
