#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct ConsumingShadowPower : PowerModel { SIM_MODEL(ConsumingShadowPower, PowerModel)
    void AfterSideTurnEnd(PlayerChoiceContext& ctx, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        Player player = Owner().Player();
        if (player.PlayerCombatState().st().orbs.empty()) return;
        for (int32_t i = 0; i < Amount(); ++i) OrbCmd::EvokeLast(ctx, player);
    }
};
}
