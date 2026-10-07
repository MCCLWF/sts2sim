// Models/Relics/StoneCalendar.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct StoneCalendar : RelicModel {
    SIM_MODEL(StoneCalendar, RelicModel)

    bool ShowCounter() const { return DisplayAmount() > -1; }

    int32_t DisplayAmount() const {
        if (!CombatManager(sim_).IsInProgress()) return -1;
        int32_t turnNumber = Owner().PlayerCombatState().TurnNumber();
        int32_t damageTurn = DynamicVars()["DamageTurn"].IntValue();
        return turnNumber >= damageTurn ? -1 : turnNumber;
    }

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature()) &&
            Owner().PlayerCombatState().TurnNumber() == DynamicVars()["DamageTurn"].IntValue())
            SetStatus(RelicStatus::Active);
    }

    void BeforeSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner().Creature())) return;
        int32_t damageTurn = DynamicVars()["DamageTurn"].IntValue();
        int32_t turnNumber = Owner().PlayerCombatState().TurnNumber();
        SetStatus(RelicStatus::Normal);
        if (turnNumber == damageTurn)
            CreatureCmd::Damage(choiceContext, Owner().Creature().CombatState().HittableEnemies(),
                                DynamicVars().Damage(), Owner().Creature());
    }

    void AfterCombatEnd(CombatRoom) const { SetStatus(RelicStatus::Normal); }

    void AfterRoomEntered(AbstractRoom room) const {
        if (room.IsCombatRoom()) SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
