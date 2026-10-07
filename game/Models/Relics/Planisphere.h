// Models/Relics/Planisphere.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Runs/RunState.h"
namespace sts2 {
struct Planisphere : RelicModel {
    SIM_MODEL(Planisphere, RelicModel)
    void AfterRoomEntered(AbstractRoom) const {
        Creature creature = Owner().Creature();
        if (creature.IsDead()) return;
        RunState run = Owner().RunState();
        if (run.CurrentRoomCount() > 1 || run.CurrentMapCol() < 0) return;
        int point = run.st().map.get_point(run.CurrentMapCol(), run.CurrentMapRow());
        if (point >= 0 && run.st().map.points[static_cast<size_t>(point)].type == MapPointType::Unknown)
            CreatureCmd::Heal(creature, DynamicVars().Heal().BaseValue());
    }
    bool IsAllowed(RunState runState) const { return IsBeforeAct3TreasureChest(runState); }
};
}
