// Models/Relics/Pantograph.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"
namespace sts2 {
struct Pantograph : RelicModel {
    SIM_MODEL(Pantograph, RelicModel)
    void AfterRoomEntered(AbstractRoom) const {
        if (Owner().Creature().IsDead()) return;
        RunState run = Owner().RunState();
        if (run.st().visited_map_coords.empty()) { SetStatus(RelicStatus::Normal); return; }
        MapCoord coord = run.st().visited_map_coords.back();
        int point = run.st().map.get_point(coord.col, coord.row);
        bool beforeBoss = point >= 0 && run.st().map.points[run.st().map.boss].parents.contains(static_cast<uint8_t>(point));
        SetStatus(beforeBoss ? RelicStatus::Active : RelicStatus::Normal);
    }
    void BeforeCombatStart() const {
        if (!Owner().Creature().IsDead() && !Owner().RunState().st().rooms.empty() &&
            Owner().RunState().st().rooms.back() == RoomType::Boss)
            CreatureCmd::Heal(Owner().Creature(), DynamicVars().Heal().BaseValue());
    }
};
}
