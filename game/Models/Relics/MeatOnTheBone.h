// Models/Relics/MeatOnTheBone.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct MeatOnTheBone : RelicModel {
    SIM_MODEL(MeatOnTheBone, RelicModel)
    bool WillHealOnCombatFinished() const {
        Creature creature = Owner().Creature();
        int32_t threshold = (Dec(creature.MaxHp()) * DynamicVars()["HpThreshold"].BaseValue() / Dec(100)).to_int();
        return creature.CurrentHp() <= threshold;
    }
    void BeforeCombatStart() const {
        if (WillHealOnCombatFinished()) SetStatus(RelicStatus::Active);
    }
    void AfterCurrentHpChanged(Creature creature, Dec) const {
        if (creature == Owner().Creature() && CombatManager(sim_).IsInProgress())
            SetStatus(WillHealOnCombatFinished() ? RelicStatus::Active : RelicStatus::Normal);
    }
    void AfterCombatVictoryEarly(CombatRoom) const {
        if (!Owner().Creature().IsDead() && WillHealOnCombatFinished()) {
            SetStatus(RelicStatus::Normal);
            CreatureCmd::Heal(Owner().Creature(), DynamicVars().Heal().BaseValue());
        }
    }
};

}  // namespace sts2
