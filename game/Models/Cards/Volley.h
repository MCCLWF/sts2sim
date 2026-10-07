#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Volley : CardModel {
    SIM_MODEL(Volley, CardModel)

    bool HasEnergyCostX() const { return true; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .WithHitCount(ResolveEnergyXValue())
            .FromCard(*this, play)
            .TargetingRandomOpponents(CombatState())
            .WithHitFx("vfx/vfx_attack_slash")
            .Execute(ctx);
    }
};

}  // namespace sts2
