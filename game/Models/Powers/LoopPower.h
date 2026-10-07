#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct LoopPower : PowerModel { SIM_MODEL(LoopPower, PowerModel)
    void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const {
        if (player != Owner().Player()) return;
        auto& orbs = player.PlayerCombatState().st().orbs;
        if (orbs.empty()) return;
        OrbModel orb(sim_, orbs[0]);
        for (int32_t i = 0; i < Amount(); ++i) OrbCmd::Passive(ctx, orb, Creature{});
    }
};
}
