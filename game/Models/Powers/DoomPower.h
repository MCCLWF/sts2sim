#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct DoomPower : PowerModel {
    SIM_MODEL(DoomPower, PowerModel)

    static CreatureList GetDoomedCreatures(const CreatureList& creatures) {
        CreatureList doomed;
        for (Creature creature : creatures) {
            PowerModel doom = creature.GetPower(PowerId::DOOM_POWER);
            if (doom && creature.CurrentHp() <= doom.Amount()) doomed.push_back(creature);
        }
        return doomed;
    }

    // The source kills the complete doomed batch before running the batch hook
    // so revival and fatal listeners observe the same set.
    static void DoomKill(const CreatureList& creatures) {
        if (creatures.empty()) return;
        sts2::CombatState combat = creatures[0].CombatState();
        for (Creature creature : creatures) CreatureCmd::Kill(creature);
        Hook::AfterDiedToDoom(combat, creatures);
    }

    void BeforeSideTurnEnd(PlayerChoiceContext&, CombatSide side, const CreatureList& participants) const {
        if (side != CombatSide::Player && ShouldDoomTrigger(participants)) DoomKill(DoomedCreatures(Owner().Side()));
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide side, const CreatureList& participants) const {
        if (side != CombatSide::Enemy && ShouldDoomTrigger(participants)) DoomKill(DoomedCreatures(Owner().Side()));
    }

private:
    CreatureList DoomedCreatures(CombatSide side) const {
        return GetDoomedCreatures(CombatState().GetCreaturesOnSide(side));
    }

    bool ShouldDoomTrigger(const CreatureList& participants) const {
        CombatManager manager(&CombatState().sim());
        if (manager.IsOverOrEnding() || !participants.contains(Owner()) || Owner().IsDead()) return false;
        if (Owner().CurrentHp() > Amount()) return false;
        CreatureList doomed = DoomedCreatures(Owner().Side());
        return !doomed.empty() && doomed[0] == Owner();
    }
};

}  // namespace sts2
