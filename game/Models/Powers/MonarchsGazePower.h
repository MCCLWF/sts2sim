#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {
struct MonarchsGazePower : PowerModel {
    SIM_MODEL(MonarchsGazePower, PowerModel)
    void AfterDamageGiven(PlayerChoiceContext& ctx, Creature dealer, const DamageResult&, ValueProp props, Creature target, CardModel) const {
        if (dealer == Owner() && IsPoweredAttack(props))
            PowerCmd::Apply(PowerId::MONARCHS_GAZE_STRENGTH_DOWN_POWER, ctx, target, Dec(Amount()), Owner(), CardModel{});
    }
};
}  // namespace sts2
