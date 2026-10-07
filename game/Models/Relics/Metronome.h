#pragma once
#include <algorithm>
#include "game/Commands/CreatureCmd.h"
#include "game/Combat/CombatManager.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct Metronome : RelicModel { SIM_MODEL(Metronome, RelicModel)
    int32_t OrbsChanneled() const { return extra(0); }
    void SetOrbsChanneled(int32_t count) const { extra(0) = count; }
    bool ShowCounter() const {
        return CombatManager(sim_).IsInProgress() && OrbsChanneled() < DynamicVars()["OrbCount"].IntValue();
    }
    int32_t DisplayAmount() const { return std::min(OrbsChanneled(), DynamicVars()["OrbCount"].IntValue()); }
    void AfterRoomEntered(AbstractRoom room) const { if (room.IsCombatRoom()) { SetOrbsChanneled(0); UpdateDisplay(); } }
    void AfterOrbChanneled(PlayerChoiceContext& ctx, Player player, OrbModel) const {
        if (player != Owner()) return;
        SetOrbsChanneled(OrbsChanneled() + 1);
        if (OrbsChanneled() == DynamicVars()["OrbCount"].IntValue())
            CreatureCmd::Damage(ctx, Owner().Creature().CombatState().HittableEnemies(), DynamicVars().Damage(), Owner().Creature());
        UpdateDisplay();
    }
    void AfterCombatEnd(CombatRoom) const { SetStatus(RelicStatus::Normal); SetOrbsChanneled(0); }
    void UpdateDisplay() const {
        SetStatus(OrbsChanneled() == DynamicVars()["OrbCount"].IntValue() - 1 ? RelicStatus::Active : RelicStatus::Normal);
    }
};
}
