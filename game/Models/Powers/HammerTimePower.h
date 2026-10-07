#pragma once

#include "game/Commands/ForgeCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct HammerTimePower : PowerModel {
    SIM_MODEL(HammerTimePower, PowerModel)
    void AfterForge(Dec amount, Player forger, AbstractModel source) const {
        if (source && source.Kind() == ModelKind::Power && source.Index() != 0 && PowerModel(&source.sim(), source.Index()).PowerId_() == PowerId::HAMMER_TIME_POWER) return;
        if (forger != Owner().Player()) return;
        for (Player player : CombatState().Players())
            if (player && player != forger && player.Creature().IsAlive()) ForgeCmd::Forge(amount, player, *this);
    }
};
}  // namespace sts2
