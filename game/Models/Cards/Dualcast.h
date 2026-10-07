// Models/Cards/Dualcast.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Dualcast : CardModel {
    SIM_MODEL(Dualcast, CardModel)
    sts2::OrbEvokeType OrbEvokeType() const { return sts2::OrbEvokeType::Front; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        if (!Owner().PlayerCombatState().st().orbs.empty()) {
            OrbCmd::EvokeNext(ctx, Owner(), false);
            OrbCmd::EvokeNext(ctx, Owner());
        }
    }
};
}
