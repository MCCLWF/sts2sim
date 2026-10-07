#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Shatter : CardModel { SIM_MODEL(Shatter, CardModel)
    sts2::OrbEvokeType OrbEvokeType() const { return sts2::OrbEvokeType::All; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
        int32_t count = static_cast<int32_t>(Owner().PlayerCombatState().st().orbs.size());
        for (int32_t i = 0; i < count; ++i) {
            OrbCmd::EvokeNext(ctx, Owner(), false);
            OrbCmd::EvokeNext(ctx, Owner());
        }
    }
};
}
