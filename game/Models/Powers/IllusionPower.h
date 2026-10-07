// Models/Powers/IllusionPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct IllusionPower : PowerModel {
    SIM_MODEL(IllusionPower, PowerModel)

    bool IsReviving() const { return extra(0) != 0; }
    void SetIsReviving(bool value) const { extra(0) = value; }

    void AfterApplied(Creature, CardModel) const {
        if (Owner().HasPower(PowerId::MINION_POWER)) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::MINION_POWER, ctx, Owner(), Dec(1), Creature(), CardModel{});
    }

    bool ShouldPowerBeRemovedOnDeath(PowerModel power) const {
        // C#: debuffs are cleared except ITemporaryPower, whose paired stat
        // adjustment must survive until its normal turn-end restoration.
        if (power.Type() != PowerType::Debuff) return false;
        switch (power.PowerId_()) {
        case PowerId::CRUSH_UNDER_POWER:
        case PowerId::DARK_SHACKLES_POWER:
        case PowerId::DYING_STAR_POWER:
        case PowerId::ENFEEBLING_TOUCH_POWER:
        case PowerId::HYPERBEAM_FOCUS_DOWN_POWER:
        case PowerId::MANGLE_POWER:
        case PowerId::MONARCHS_GAZE_STRENGTH_DOWN_POWER:
        case PowerId::PIERCING_WAIL_POWER:
        case PowerId::SHACKLING_POTION_POWER:
            return false;
        default:
            return true;
        }
    }

    bool ShouldAllowHitting(Creature creature) const {
        return creature != Owner() || !IsReviving();
    }

    bool ShouldCreatureBeRemovedFromCombatAfterDeath(Creature creature) const {
        return creature != Owner();
    }

    void AfterDeath(PlayerChoiceContext&, Creature creature, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented || creature != Owner()) return;
        MonsterModel monster = Owner().Monster();
        if (!monster) return;
        SetIsReviving(true);
        std::string_view followUp = monster.LastLoggedMoveId();
        int8_t revive = monster.MachineDef().Find("REVIVE_MOVE");
        if (revive >= 0) {
            // The game creates a fresh MoveState for every death; the static
            // simulator node therefore needs the same initially-unperformed state.
            monster.st().machine.performed_at_least_once &= ~(uint32_t{1} << revive);
        }
        monster.SetMoveImmediate("REVIVE_MOVE", true);
        int8_t next = monster.MachineDef().Find(followUp);
        if (revive >= 0 && next >= 0) monster.st().machine.follow_up[revive] = next;
    }

    void AfterCombatEnd(CombatRoom) const {}

    void Revive() const {
        SetIsReviving(false);
        Creature owner = Owner();
        CreatureCmd::Heal(owner, Dec(owner.MaxHp() - owner.CurrentHp()));
    }
};

}  // namespace sts2
