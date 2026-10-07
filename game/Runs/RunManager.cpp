// Runs/RunManager.cs
#include "game/Runs/RunManager.h"

#include "engine/run_setup.h"
#include "engine/sim/gc.h"
#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Models/EncounterModel.h"
#include "game/Models/EventModel.h"
#include "game/Odds/UnknownMapPointOdds.h"
#include "game/Rooms/AbstractRoom.h"
#include "game/Rooms/TreasureRoom.h"
#include "game/Entities/Merchant/MerchantInventory.h"
#include "game/Entities/RestSite/RestSiteOption.h"
#include "game/Hooks/HookGuard.h"
#include "game/Runs/RunFlow.h"

namespace sts2::RunManager {

namespace {

// RunState.PushRoom / PopCurrentRoom.
void PushRoom(Sim* sim, RoomType type) { sim->g->run.rooms.push_back(type); }

void ExitCurrentRoom(Sim* sim) {
    RunData& run = sim->g->run;
    if (run.rooms.empty()) throw GameError("There are no rooms to pop.");
    // RewardsSetSynchronizer.BeforeLeavingRoom: skip the sets still on screen.
    RewardsSet set(sim);
    if (set) set.Skip();
    // BeforeLeavingRoom clears the pending reward stack. ClearScreens drops
    // its retained display entries too; completed entries remain queryable
    // until this actual room exit, never after their objects are reclaimed.
    run.rewards = RewardsSetState{};
    RoomType room = run.rooms.back();
    run.rooms.pop_back();
    if (room == RoomType::Monster || room == RoomType::Elite || room == RoomType::Boss) {
        CombatManager(sim).Reset(/*graceful=*/true);  // CombatRoom.Exit (not pre-finished)
    } else if (room == RoomType::Event) {
        // EventRoom.Exit: EventSynchronizer.BeforeExitingRoom, EnsureCleanup.
        EventModel(sim).EnsureCleanup();
        if (sim->g->combat.precreated_for_event_layout) sim->g->combat = CombatData{};
        if (run.room_event == EventId::FAKE_MERCHANT) run.merchant = MerchantState{};
    } else if (room == RoomType::RestSite) {
        // RestSiteRoom.Exit (AfterAllRestSitesCompleted: single player, done).
        RestSite::BeforeLocalRestSiteExited(sim);
    } else if (room == RoomType::Treasure) {
        TreasureRoom::OnRoomExited(sim);
    } else if (room == RoomType::Shop) {
        // MerchantRoom.Exit: map point history only (TestMode skips it). The
        // room and its inventory are dropped.
        sim->g->run.merchant = MerchantState{};
    }
}

void ExitCurrentRooms(Sim* sim) {
    while (!sim->g->run.rooms.empty()) ExitCurrentRoom(sim);
    sim->g->run.combat_extra_rewards.clear();
    CollectGarbage(*sim->g);  // the left rooms' objects are unreachable now
}

uint16_t Bit(RoomType t) { return static_cast<uint16_t>(1u << static_cast<unsigned>(t)); }

// BuildRoomTypeBlacklist(State.CurrentMapPointHistoryEntry, State.CurrentMapPoint?.Children):
// no shop right after a shop, nor when every next point is a shop.
uint16_t BuildRoomTypeBlacklist(Sim* sim) {
    const RunData& run = sim->g->run;
    uint16_t blacklist = 0;
    bool previousHadShop = run.total_floor > 0 && (run.current_entry_rooms & Bit(RoomType::Shop)) != 0;
    bool allNextShops = false;
    if (!run.visited_map_coords.empty()) {
        MapCoord c = run.visited_map_coords.back();
        int p = run.map.get_point(c.col, c.row);
        if (p >= 0) {
            const MapPoint& point = run.map.points[static_cast<size_t>(p)];
            allNextShops = point.children.size() > 0 &&
                           !point.children.any([&](uint8_t q) { return run.map.points[q].type != MapPointType::Shop; });
        }
    }
    if (previousHadShop || allNextShops) blacklist |= Bit(RoomType::Shop);
    return blacklist;
}

// RollRoomTypeFor. The tutorial override needs TestMode off and a first run
// (never, on the fully unlocked profile).
RoomType RollRoomTypeFor(Sim* sim, MapPointType pointType) {
    switch (pointType) {
    case MapPointType::Unassigned: return RoomType::Unassigned;
    case MapPointType::Unknown: return UnknownMapPointOddsOps::Roll(sim, BuildRoomTypeBlacklist(sim));
    case MapPointType::Shop: return RoomType::Shop;
    case MapPointType::Treasure: return RoomType::Treasure;
    case MapPointType::RestSite: return RoomType::RestSite;
    case MapPointType::Monster: return RoomType::Monster;
    case MapPointType::Elite: return RoomType::Elite;
    case MapPointType::Boss: return RoomType::Boss;
    case MapPointType::Ancient: return RoomType::Event;
    default: throw GameError("RollRoomTypeFor: point type out of range");
    }
}

// ActModel.PullNextEncounter (RoomSet.NextNormalEncounter / NextEliteEncounter / NextBossEncounter).
EncounterId PullNextEncounter(Sim* sim, RoomType roomType) {
    RunData& run = sim->g->run;
    const RoomSet& rooms = run.acts[static_cast<size_t>(run.current_act_index)].rooms;
    switch (roomType) {
    case RoomType::Monster:
        return rooms.normal_encounters[static_cast<size_t>(rooms.normal_encounters_visited) % rooms.normal_encounters.size()];
    case RoomType::Elite:
        return rooms.elite_encounters[static_cast<size_t>(rooms.elite_encounters_visited) % rooms.elite_encounters.size()];
    case RoomType::Boss:
        return rooms.boss_encounters_visited != 0 && rooms.second_boss != EncounterId::NONE ? rooms.second_boss : rooms.boss;
    default: throw GameError("PullNextEncounter: room type out of range");
    }
}

// ActModel.MarkRoomVisited (RoomSet.MarkVisited).
void MarkRoomVisited(Sim* sim, RoomType roomType) {
    RunData& run = sim->g->run;
    RoomSet& rooms = run.acts[static_cast<size_t>(run.current_act_index)].rooms;
    switch (roomType) {
    case RoomType::Monster: ++rooms.normal_encounters_visited; break;
    case RoomType::Elite: ++rooms.elite_encounters_visited; break;
    case RoomType::Event: ++rooms.events_visited; break;
    case RoomType::Boss: ++rooms.boss_encounters_visited; break;
    default: break;
    }
}

// ActModel.PullNextEvent: RoomSet.EnsureNextEventIsValid, Hook.ModifyNextEvent,
// RunState.AddVisitedEvent.
EventId PullNextEvent(Sim* sim) {
    RunData& run = sim->g->run;
    RoomSet& rooms = run.acts[static_cast<size_t>(run.current_act_index)].rooms;
    if (rooms.events.empty()) throw GameError("PullNextEvent: the act has no events");
    RunState state(sim);
    for (size_t i = 0; i < rooms.events.size(); ++i) {
        EventId next = rooms.events[static_cast<size_t>(rooms.events_visited) % rooms.events.size()];
        if (EventModel::Canonical(sim, next).IsAllowed(state) && !run.visited_event_ids.contains(next)) break;
        ++rooms.events_visited;
    }
    EventId eventId = rooms.events[static_cast<size_t>(rooms.events_visited) % rooms.events.size()];
    EventModel eventModel = Hook::ModifyNextEvent(state, EventModel::Canonical(sim, eventId));
    eventId = eventModel.EventId_();
    if (!run.visited_event_ids.contains(eventId)) run.visited_event_ids.push_back(eventId);
    return eventId;
}

// new CombatRoom(encounter.ToMutable(), State): new CombatState(encounter, runState).
void CreateCombatRoom(Sim* sim, EncounterId encounter) {
    CombatData& c = sim->g->combat;
    c = CombatData{};
    c.exists = true;
    c.encounter = encounter;
    c.round_number = 1;
    c.current_side = CombatSide::Player;
}

// CombatRoom.EnterInternal -> StartCombat.
void EnterCombatRoom(Sim* sim) {
    RunState runState(sim);
    CombatState combatState(sim);
    if (combatState.Players().empty()) {
        for (Player item : runState.Players()) combatState.AddPlayer(item);
    }
    // StartCombat
    EncounterModel encounter(sim);
    if (!sim->g->combat.precreated_for_event_layout) {
        MonsterSpawnList monsters = encounter.GenerateMonstersWithSlots(runState);
        for (const MonsterSpawn& spawn : monsters) {
            Creature creature = combatState.CreateCreature(spawn.monster, CombatSide::Enemy, spawn.slot);
            combatState.AddCreature(creature);
            // CurrentMapPointHistoryEntry...MonsterIds: run history only.
        }
    }
    CombatManager cm(sim);
    cm.SetUpCombat(combatState);
    Hook::AfterRoomEntered(runState, CombatRoom(sim, encounter.RoomType_()));
    // AfterCombatRoomLoaded starts the turn loop; it runs after this room
    // entry finishes (see EnterRoomInternal).
}

// EventRoom.EnterInternal: EventSynchronizer.BeginEvent (the canonical event's
// ToMutable, BeginEvent), Hook.AfterRoomEntered, AfterEventStarted.
void EnterEventRoom(Sim* sim) {
    RunState state(sim);
    EventModel ev = EventModel::ToMutable(sim, sim->g->run.room_event);
    ev.BeginEvent(Player(sim, 1), /*isPreFinished=*/false);
    if (const EncounterId encounterId = ev.CanonicalEncounter(); encounterId != EncounterId::NONE) {
        // EventCombatSynchronizer.InitializeForEvent builds the layout before
        // the first event choice. Its monsters and Niche RNG draws are kept
        // for the subsequent fight, if that option is chosen.
        CreateCombatRoom(sim, encounterId);
        CombatState layout(sim);
        for (Player player : state.Players()) layout.AddPlayer(player);
        EncounterModel encounter(sim);
        MonsterSpawnList monsters = encounter.GenerateMonstersWithSlots(state);
        for (const MonsterSpawn& spawn : monsters)
            layout.AddCreature(layout.CreateCreature(spawn.monster, CombatSide::Enemy, spawn.slot));
        sim->g->combat.precreated_for_event_layout = true;
    }
    Hook::AfterRoomEntered(state, AbstractRoom(sim, RoomType::Event));
    ev.AfterEventStarted();
}

// EnterRoomInternal (not restoring a room stack base).
void EnterRoomInternal(Sim* sim, RoomType roomType) {
    RunState state(sim);
    PushRoom(sim, roomType);
    // EnterRoomInternal explicitly excludes MapRoom from BeforeRoomEntered.
    // MapRoom.Enter/Exit only create/remove presentation nodes, and it has no
    // AfterRoomEntered hook. MarkRoomVisited(Map) is a no-op.
    if (roomType == RoomType::Map) return;
    if (roomType == RoomType::Event) {
        Hook::BeforeRoomEntered(state, AbstractRoom(sim, roomType));
        EnterEventRoom(sim);
        if (state.CurrentRoomCount() == 1) MarkRoomVisited(sim, roomType);
        return;
    }
    bool combat = roomType == RoomType::Monster || roomType == RoomType::Elite || roomType == RoomType::Boss;
    if (!combat) {
        Hook::BeforeRoomEntered(state, AbstractRoom(sim, roomType));
        switch (roomType) {
        case RoomType::RestSite:
            // RestSiteRoom.EnterInternal
            RestSite::BeginRestSite(sim);
            Hook::AfterRoomEntered(state, AbstractRoom(sim, roomType));
            break;
        case RoomType::Treasure:
            // TreasureRoom.EnterInternal
            Hook::AfterRoomEntered(state, AbstractRoom(sim, roomType));
            TreasureRoom::BeginRelicPicking(sim);
            break;
        case RoomType::Shop:
            // MerchantRoom.EnterInternal
            Merchant::CreateForNormalMerchant(sim);
            Hook::AfterRoomEntered(state, AbstractRoom(sim, roomType));
            break;
        default: throw Unported("entering this room type is not ported yet");
        }
        if (state.CurrentRoomCount() == 1) MarkRoomVisited(sim, roomType);
        return;
    }
    Hook::BeforeRoomEntered(state, CombatRoom(sim, roomType));
    EnterCombatRoom(sim);
    if (state.CurrentRoomCount() == 1) MarkRoomVisited(sim, roomType);
    // The turn loop (StartCombatInternal) up to the player's first wait. In
    // the game it starts inside room.Enter; nothing between there and here
    // (MarkRoomVisited) is read by combat start.
    CombatManager(sim).StartCombat();
}

// EnterMapPointInternal (no pre-finished room, no network sync).
void EnterMapPointInternal(Sim* sim, int32_t actFloor, MapPointType pointType) {
    RunData& run = sim->g->run;
    run.act_floor = static_cast<int32_t>(actFloor);
    run.current_map_affected_by_fur_coat = false;
    ExitCurrentRooms(sim);
    RoomType roomType = RollRoomTypeFor(sim, pointType);
    // CreateRoom
    switch (roomType) {
    case RoomType::Monster:
    case RoomType::Elite:
    case RoomType::Boss: CreateCombatRoom(sim, PullNextEncounter(sim, roomType)); break;
    case RoomType::Event:  // new EventRoom(Ancient ? Act.PullAncient() : Act.PullNextEvent(State))
        run.room_event = pointType == MapPointType::Ancient
                             ? run.acts[static_cast<size_t>(run.current_act_index)].rooms.ancient
                             : PullNextEvent(sim);
        break;
    case RoomType::Treasure:  // new TreasureRoom(State.CurrentActIndex)
        if (run.current_act_index < 0 || run.current_act_index > 2) throw GameError("actIndex must be between 0 and 2");
        break;
    case RoomType::Shop:      // new MerchantRoom()
    case RoomType::RestSite:  // new RestSiteRoom()
        break;
    default: throw Unported("creating this room type is not ported yet");
    }
    // AppendToMapPointHistory
    ++run.total_floor;
    run.current_entry_rooms = Bit(roomType);
    // EnterRoom: ExitCurrentRooms (none left), EnterRoomInternal
    EnterRoomInternal(sim, roomType);
    // AfterMapLocationChanged: UI and network only.
}

}  // namespace

void EnterEventCombat(Sim* sim, EncounterId encounter, const StaticVec<RewardRef, 8>& extraRewards,
                      bool shouldResumeAfterCombat) {
    if (sim->g->run.rooms.empty() || sim->g->run.rooms.back() != RoomType::Event)
        throw GameError("EnterEventCombat requires an event room");
    if (!EventModel(sim).IsShared()) throw GameError("EnterEventCombat requires a shared event");
    if (shouldResumeAfterCombat && data(sim->g->run.room_event).layout == EventLayoutType::Combat)
        throw GameError("cannot resume a combat-layout event after combat");
    RoomType roomType = data(encounter).room_type;
    if (!sim->g->combat.precreated_for_event_layout || sim->g->combat.encounter != encounter)
        CreateCombatRoom(sim, encounter);
    sim->g->combat.should_resume_parent_event_after_combat = shouldResumeAfterCombat;
    sim->g->run.combat_extra_rewards = extraRewards;
    sim->g->run.current_entry_rooms |= Bit(roomType);
    EnterRoomInternal(sim, roomType);
    sim->g->run.phase = RunPhase::Combat;
}

void ResumeParentEventAfterCombat(Sim* sim) {
    RunData& run = sim->g->run;
    if (run.rooms.size() < 2 || run.rooms.back() == RoomType::Event ||
        run.rooms[run.rooms.size() - 2] != RoomType::Event ||
        !sim->g->combat.should_resume_parent_event_after_combat)
        throw GameError("ResumeParentEventAfterCombat requires a resumable event combat");
    // RunManager.ResumePreviousRoom pops and exits the combat before asking
    // EventRoom to resume. The value handle retains the exited encounter's
    // facts, including its custom state and any extra rewards.
    AbstractRoom exitedRoom(sim, run.rooms.back());
    ExitCurrentRoom(sim);
    run.combat_extra_rewards.clear();
    run.phase = RunPhase::Event;
    EventModel event(sim);
    event.Resume(exitedRoom);
    if (run.phase == RunPhase::Event && event.IsFinished()) run.phase = RunPhase::Map;
}

void SetActInternal(Sim* sim, int32_t actIndex) {
    RunData& run = sim->g->run;
    if (actIndex < 0 || actIndex >= static_cast<int32_t>(run.acts.size()))
        throw GameError("SetActInternal: act index out of range");
    // RunState.CurrentActIndex setter resets ActFloor only on a new act.
    if (run.current_act_index != actIndex) run.act_floor = 0;
    run.current_act_index = actIndex;
    run.visited_map_coords.clear();  // ClearVisitedMapCoordsDebug
    run.current_map_affected_by_fur_coat = false;
    run.odds = UnknownMapPointOdds{};  // ResetToBase (base odds: only the DeadlyEvents modifier changes them)
    GenerateMap(sim);
}

void EnterAct(Sim* sim, int32_t actIndex) {
    // RunManager.EnterAct: fades, loading scopes and ActEntered UI signals
    // contain no gameplay work. Preserve room exit and hook/RNG order.
    ExitCurrentRooms(sim);
    SetActInternal(sim, actIndex);
    if (actIndex == 0 && sim->g->run.started_with_neow)
        EnterMapCoord(sim, sim->g->run.map.points[sim->g->run.map.start].coord);
    else
        EnterRoomInternal(sim, RoomType::Map);
    RunFlow::AfterRoomEntered(sim);
    Hook::AfterActEntered(RunState(sim));
}

void EnterNextAct(Sim* sim) {
    auto& run = sim->g->run;
    if (run.current_act_index < static_cast<int32_t>(run.acts.size()) - 1) {
        EnterAct(sim, run.current_act_index + 1);
        return;
    }
    if (!run.rooms.empty() && run.rooms.back() == RoomType::Event && run.room_event == EventId::THE_ARCHITECT)
        throw GameError("EnterNextAct called at victory room! WinRun should be called instead");
    // The last act proceeds to a real event room. Its not-yet-ported event
    // behavior remains an explicit Unported at the normal dispatch boundary;
    // an empty map or empty final Boss reward must not be called a victory.
    ExitCurrentRooms(sim);
    run.room_event = EventId::THE_ARCHITECT;
    EnterRoomInternal(sim, RoomType::Event);
    RunFlow::AfterRoomEntered(sim);
}

void Abandon(Sim* sim) {
    // AbandonInternal closes screens (presentation), then sets IsAbandoned.
    sim->g->run.is_abandoned = true;
    // GuaranteeKillAllPlayers; the inter-player scaled wait is presentation.
    for (Player player : RunState(sim).Players()) {
        CreatureCmd::Kill(player.Creature(), /*force=*/true);
    }
    RunFlow::SettleGameOver(sim);
}

void WinRun(Sim* sim) {
    RunData& run = sim->g->run;
    if (run.rooms.empty() || run.rooms.back() != RoomType::Event || run.room_event != EventId::THE_ARCHITECT)
        throw GameError("WinRun requires The Architect victory room");
    // RunManager.WinRun calls OnEnded(isVictory: true) before
    // GuaranteeKillAllPlayers. Preserve the result before the presentation
    // death hooks run so Env clients can distinguish victory from loss.
    run.is_victory = true;
    run.phase = RunPhase::Ended;
    CreatureCmd::Kill(Player(sim, 1).Creature(), /*force=*/true);
}

void GenerateMap(Sim* sim) {
    RunData& run = sim->g->run;
    // Apply map transforms, then the in-place AfterMapGenerated callbacks.
    ActMap map = Hook::ModifyGeneratedMap(RunState(sim), generate_map_for_act(run, run.current_act_index),
                                          run.current_act_index);
    Hook::AfterMapGenerated(RunState(sim), map, run.current_act_index);
    if (!run.started_with_neow && run.current_act_index == 0) map.points[map.start].type = MapPointType::Monster;
    run.map = map;
    // RemoveStaleVisitedMapCoords: none right after SetActInternal.
}

void EnterMapCoord(Sim* sim, MapCoord coord) {
    RunState state(sim);
    if (!state.AddVisitedMapCoord(coord)) return;
    const ActMap& map = sim->g->run.map;
    int p = map.get_point(coord.col, coord.row);
    if (p < 0) throw GameError("EnterMapCoord: no map point at the coord");
    EnterMapPointInternal(sim, coord.row + 1, map.points[static_cast<size_t>(p)].type);
}

void EnterMapCoordDebug(Sim* sim, MapCoord coord, EncounterId encounter) {
    RunState state(sim);
    state.AddVisitedMapCoord(coord);
    // EnterRoomDebug
    ExitCurrentRooms(sim);
    RoomType roomType = data(encounter).room_type;
    ++sim->g->run.total_floor;  // AppendToMapPointHistory
    sim->g->run.current_entry_rooms = Bit(roomType);
    CreateCombatRoom(sim, encounter);
    EnterRoomInternal(sim, roomType);
}

}  // namespace sts2::RunManager
