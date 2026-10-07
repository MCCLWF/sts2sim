#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Malaise : CardModel {
    SIM_MODEL(Malaise, CardModel)

    sts2::TargetType TargetType() const { return sts2::TargetType::AnyEnemy; }
    bool HasEnergyCostX() const { return true; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        int32_t amount = ResolveEnergyXValue() + (CurrentUpgradeLevel() > 0 ? 1 : 0);
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, play->Target, Dec(-amount), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, play->Target, Dec(amount), Owner().Creature(), *this);
    }
};

}  // namespace sts2
