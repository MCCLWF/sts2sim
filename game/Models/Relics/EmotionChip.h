#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Combat/CombatManager.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct EmotionChip : RelicModel { SIM_MODEL(EmotionChip, RelicModel)
    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result, ValueProp, Creature, CardModel) const {
        if (!CombatManager(sim_).IsInProgress() || target != Owner().Creature() || result.UnblockedDamage <= 0) return;
        SetStatus(RelicStatus::Active);
    }
    void AfterPlayerTurnStart(PlayerChoiceContext& ctx, Player player) const {
        if (player != Owner()) return;
        SetStatus(RelicStatus::Normal);
        if (!Owner().Creature().CombatState().st().player_nonfully_blocked_damage_previous_turn) return;
        auto orbs = Owner().PlayerCombatState().st().orbs;
        for (OrbRef ref : orbs) OrbCmd::Passive(ctx, OrbModel(sim_, ref), Creature{}, true);
    }
    void AfterCombatEnd(CombatRoom) const { SetStatus(RelicStatus::Normal); }
};
}
