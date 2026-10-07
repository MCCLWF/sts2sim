// Rooms/TreasureRoom.cs with the single-player parts of
// Multiplayer/Game/TreasureRoomRelicSynchronizer.cs and
// OneOffSynchronizer.DoTreasureRoomRewards. The relic picking lives in
// RunData::treasure.
//
// The client's order (NTreasureRoom.OpenChest): entering the room begins the
// relic picking; opening the chest gives the gold (DoNormalRewards) and the
// extra rewards; then the player picks the relic or skips. Awarding the
// picked relic is the relic collection node's (NTreasureRoomRelicCollection
// .AnimateRelicAwards: RelicCmd.Obtain).
#pragma once

#include "game/All.h"

namespace sts2::TreasureRoom {

void BeginRelicPicking(Sim* sim);
int32_t DoNormalRewards(Sim* sim);
void DoExtraRewardsIfNeeded(Sim* sim);
// PickRelicLocally(index) (-1: SkipRelicLocally), then OnPicked (the
// PickRelicAction) and the award.
void PickRelic(Sim* sim, int32_t index);
void OnRoomExited(Sim* sim);

}  // namespace sts2::TreasureRoom
