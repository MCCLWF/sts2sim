#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Quadcast : CardModel { SIM_MODEL(Quadcast, CardModel)
    sts2::OrbEvokeType OrbEvokeType() const { return sts2::OrbEvokeType::Front; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        if (Owner().PlayerCombatState().st().orbs.empty()) return;
        int32_t count = DynamicVars().Repeat().IntValue();
        for (int32_t i = 0; i < count; ++i) OrbCmd::EvokeNext(ctx, Owner(), i == count - 1);
    }
};
}
