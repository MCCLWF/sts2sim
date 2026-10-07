// Models/Relics/RedSkull.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct RedSkull : RelicModel {
    SIM_MODEL(RedSkull, RelicModel)

    void ModifyStrengthIfNecessary() const {
        Creature creature = Owner().Creature();
        bool above = Dec(creature.CurrentHp()) >
                     Dec(creature.MaxHp()) * DynamicVars()["HpThreshold"].BaseValue() / Dec(100);
        SetStatus(above ? RelicStatus::Normal : RelicStatus::Active);
        bool applied = extra(0) != 0;
        if (above == applied) {
            PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
            Dec strength = DynamicVars().Strength().BaseValue();
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, creature, above ? -strength : strength, creature, CardModel{});
            extra(0) = above ? 0 : 1;
        }
    }

    void AfterRoomEntered(AbstractRoom room) const {
        if (room.IsCombatRoom()) ModifyStrengthIfNecessary();
    }
    void AfterCurrentHpChanged(Creature, Dec) const {
        if (CombatManager(sim_).IsInProgress()) ModifyStrengthIfNecessary();
    }
    void AfterCombatEnd(CombatRoom) const {
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
