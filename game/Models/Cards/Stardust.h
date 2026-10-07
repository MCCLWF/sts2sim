#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Stardust : CardModel {
    SIM_MODEL(Stardust, CardModel)
    bool HasStarCostX() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(ResolveStarXValue()).FromCard(*this, play).TargetingRandomOpponents(CombatState()).Execute(ctx);
    }
};
}  // namespace sts2
