// Runs/RunManager.cs: moving through the run (acts, map points, rooms).
// Networking, saving, loading screens, music and the action queue's
// pause / unpause are dropped; the combat turn loop runs synchronously up to
// the player's first wait (CombatManager::StartCombat).
#pragma once

#include "engine/map.h"
#include "game/All.h"

namespace sts2::RunManager {

// Ordinary act entry: exit rooms, set/generate this act's map, enter the map
// (or the initial Neow event), then AfterActEntered. RoomSet was preselected
// at run setup and is not generated again.
void EnterAct(Sim* sim, int32_t actIndex);
// RunManager.EnterNextAct; the caller performs ActChangeSynchronizer's
// ActFloor increment. The last act enters TheArchitect, never a fake victory.
void EnterNextAct(Sim* sim);
// RunManager.WinRun: record the terminal victory, then apply the game's
// post-run forced player death used by the game-over presentation.
void WinRun(Sim* sim);
// Singleplayer AbandonInternal / GuaranteeKillAllPlayers: forced death,
// including the ordinary death hooks, after marking the run abandoned.
void Abandon(Sim* sim);

// SetActInternal: CurrentActIndex, visited coords cleared, unknown-room odds
// reset, GenerateMap.
void SetActInternal(Sim* sim, int32_t actIndex);
void GenerateMap(Sim* sim);

// EnterMapCoord: nothing if the coord was already visited this act.
void EnterMapCoord(Sim* sim, MapCoord coord);

// EnterMapCoordDebug(coord, roomType, pointType, encounter): enters `coord`
// but fights `encounter` (oracle `combat --encounter`, for L3 of any encounter).
void EnterMapCoordDebug(Sim* sim, MapCoord coord, EncounterId encounter);

// The event stays beneath the combat room on the room stack.
void EnterEventCombat(Sim* sim, EncounterId encounter,
                      const StaticVec<RewardRef, 8>& extraRewards = {},
                      bool shouldResumeAfterCombat = false);

// RunManager.ResumePreviousRoom after a nested event combat. The exited
// CombatRoom value retains its encounter state for EventModel.Resume.
void ResumeParentEventAfterCombat(Sim* sim);

}  // namespace sts2::RunManager
