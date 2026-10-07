// Models/Relics/WingedBoots.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"
namespace sts2 {
struct WingedBoots : RelicModel {
    SIM_MODEL(WingedBoots, RelicModel)
    bool IsAllowed(RunState runState) const { return runState.Players().size() == 1; }
    bool IsUsedUp() const { return extra(0) >= 3; }
    bool ShowCounter() const { return !IsUsedUp(); }
    int32_t DisplayAmount() const { return 3 - extra(0); }
    bool ShouldAllowFreeTravel() const { return !IsUsedUp(); }
    void AfterRoomEntered(AbstractRoom room) const {
        (void)room;
        if (IsUsedUp() || sim_->g->run.rooms.size() > 1) return;
        const auto& visited = sim_->g->run.visited_map_coords;
        if (visited.size() <= 1) return;
        const MapCoord previous = visited[visited.size() - 2];
        const MapCoord current = visited.back();
        const int previousIndex = sim_->g->run.map.get_point(previous.col, previous.row);
        const int currentIndex = sim_->g->run.map.get_point(current.col, current.row);
        if (previousIndex < 0 || currentIndex < 0) return;
        const auto& children = sim_->g->run.map.points[static_cast<size_t>(previousIndex)].children;
        if (children.contains(static_cast<uint8_t>(currentIndex))) return;
        ++extra(0);
        if (IsUsedUp()) SetStatus(RelicStatus::Disabled);
    }
};
}
