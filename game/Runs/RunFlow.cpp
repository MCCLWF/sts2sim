// See RunFlow.h. Sources: Nodes/Screens/Map/NMapScreen.cs (travelable points),
// Map/MapTravel.cs, Nodes/Combat/NCombatUi.cs (OnCombatWon),
// Rooms/CombatRoom.cs (OfferRoomEndRewards), Nodes/Rewards (the screen).
#include "game/Runs/RunFlow.h"

#include <algorithm>

#include "game/Combat/CombatManager.h"
#include "game/GameActions/ActionExecutor.h"
#include "game/Commands/PotionCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Models/EncounterModel.h"
#include "game/Runs/RunManager.h"
#include "game/Entities/RestSite/RestSiteOption.h"
#include "game/Rooms/TreasureRoom.h"

namespace sts2::RunFlow {

namespace {

bool InCombatRoom(Sim* sim) {
    const auto& rooms = sim->g->run.rooms;
    if (rooms.empty()) return false;
    RoomType t = rooms.back();
    return t == RoomType::Monster || t == RoomType::Elite || t == RoomType::Boss;
}

void ProceedFromTerminalCombat(Sim* sim) {
    RunData& run = sim->g->run;
    if (InCombatRoom(sim) && run.rooms.size() >= 2 &&
        run.rooms[run.rooms.size() - 2] == RoomType::Event &&
        sim->g->combat.should_resume_parent_event_after_combat) {
        RunManager::ResumeParentEventAfterCombat(sim);
        return;
    }
    run.phase = RunPhase::Map;
}

// CombatRoom.OfferRoomEndRewards: RewardsCmd.GenerateForRoomEnd, then
// Hook.BeforeCombatRewardOffered, then Offer.
void OfferRoomEndRewards(Sim* sim) {
    RoomType room = sim->g->run.rooms.back();
    RewardsSet set = RewardsSet::NewForRoom(sim, room);
    set.GenerateWithoutOffering();
    Hook::BeforeCombatRewardOffered(set, RunState(sim), CombatRoom(sim, room));
    if (set.Offer()) sim->g->run.phase = RunPhase::Rewards;
    else ProceedFromTerminalCombat(sim);
}

// After a combat action: the combat is over when it no longer is in progress.
void AfterCombatAction(Sim* sim) {
    CombatManager cm(sim);
    if (cm.IsInProgress()) return;
    if (Player(sim, 1).Creature().IsDead()) {
        sim->g->run.is_victory = false;
        sim->g->run.phase = RunPhase::Ended;  // the run is lost
        return;
    }
    // NCombatUi.OnCombatWon skips the rewards screen entirely when the
    // encounter declines normal rewards. An event can then resume its parent
    // room and offer its own custom rewards (Battleworn Dummy).
    if (!EncounterModel(sim).ShouldGiveRewards()) {
        ProceedFromTerminalCombat(sim);
        return;
    }
    OfferRoomEndRewards(sim);
}

}  // namespace

// The phase after entering a room.
void AfterRoomEntered(Sim* sim) {
    const auto& rooms = sim->g->run.rooms;
    if (!rooms.empty() && rooms.back() == RoomType::Event) {
        if (sim->g->run.room_event == EventId::FAKE_MERCHANT) {
            // The source uses a custom merchant screen and no EventOption list.
            sim->g->run.phase = RunPhase::Shop;
            return;
        }
        // A finished event shows its last page with the UI's proceed button.
        sim->g->run.phase = EventModel(sim).IsFinished() ? RunPhase::Map : RunPhase::Event;
        return;
    }
    if (InCombatRoom(sim)) {
        sim->g->run.phase = CombatManager(sim).IsInProgress() ? RunPhase::Combat : RunPhase::Map;
        if (sim->g->run.phase == RunPhase::Map) AfterCombatAction(sim);
        return;
    }
    switch (rooms.empty() ? RoomType::Unassigned : rooms.back()) {
    case RoomType::Map: sim->g->run.phase = RunPhase::Map; return;
    case RoomType::RestSite: sim->g->run.phase = RunPhase::RestSite; return;
    case RoomType::Shop: sim->g->run.phase = RunPhase::Shop; return;
    case RoomType::Treasure:
        // NTreasureRoom.OpenChest (the environment always opens the chest).
        TreasureRoom::DoNormalRewards(sim);
        TreasureRoom::DoExtraRewardsIfNeeded(sim);
        sim->g->run.phase = sim->g->run.treasure.picking ? RunPhase::Treasure : RunPhase::Map;
        return;
    default: throw Unported("entering this room type is not ported yet");
    }
}

void StartRun(Sim* sim) {
    RunManager::EnterAct(sim, 0);
}

// CreatureCmd.Kill: once every player is dead it calls
// RunManager.OnEnded(isVictory: false) and shows the game-over screen, in
// combat or not. A combat loss reaches the end through AfterCombatAction;
// this is the same end for a death outside combat (an event's HP loss), where
// the screen the action left open is covered by the game-over screen.
void SettleGameOver(Sim* sim) {
    RunData& run = sim->g->run;
    if (run.phase == RunPhase::None || run.phase == RunPhase::Ended) return;
    if (CombatManager(sim).IsInProgress()) return;
    if (!Player(sim, 1).Creature().IsDead()) return;
    run.is_victory = false;
    run.phase = RunPhase::Ended;
}

StaticVec<MapCoord, 8> MapOptions(Sim* sim) {
    const RunData& run = sim->g->run;
    const ActMap& map = run.map;
    StaticVec<MapCoord, 8> out;
    if (run.visited_map_coords.empty()) {
        out.push_back(map.points[map.start].coord);
        return out;
    }
    MapCoord mapCoord = run.visited_map_coords.back();
    if (map.second_boss >= 0 && mapCoord == map.points[map.boss].coord) {
        out.push_back(map.points[static_cast<size_t>(map.second_boss)].coord);
        return out;
    }
    if (mapCoord.row == map.rows - 1) {
        out.push_back(map.points[map.boss].coord);
        return out;
    }
    int p = map.get_point(mapCoord.col, mapCoord.row);
    if (p < 0) throw GameError("MapOptions: the current coord is not on the map");
    if (Hook::ShouldAllowFreeTravel(RunState(sim))) {
        for (int c = 0; c < map.cols; ++c) {
            int q = map.point_at(c, mapCoord.row + 1);
            if (q >= 0) out.push_back(map.points[static_cast<size_t>(q)].coord);
        }
    } else {
        map.points[static_cast<size_t>(p)].children.for_each([&](uint8_t child) { out.push_back(map.points[child].coord); });
    }
    // By (col, row): insertion sort (at most a handful of points).
    auto less = [](MapCoord a, MapCoord b) { return a.col != b.col ? a.col < b.col : a.row < b.row; };
    for (size_t i = 1; i < out.size(); ++i) {
        for (size_t j = i; j > 0 && less(out[j], out[j - 1]); --j) std::swap(out[j], out[j - 1]);
    }
    return out;
}

bool IsBossPoint(Sim* sim, MapCoord coord) {
    const ActMap& map = sim->g->run.map;
    if (map.points[map.boss].coord == coord) return true;
    return map.second_boss >= 0 && map.points[static_cast<size_t>(map.second_boss)].coord == coord;
}

void ChooseMapPoint(Sim* sim, MapCoord coord) {
    if (sim->g->run.phase != RunPhase::Map) throw GameError("ChooseMapPoint: not on the map");
    if (!MapOptions(sim).contains(coord)) throw GameError("ChooseMapPoint: the point cannot be travelled to");
    sim->g->run.phase = RunPhase::None;
    RunManager::EnterMapCoord(sim, coord);
    AfterRoomEntered(sim);
}

bool PlayCard(Sim* sim, int32_t handIndex, int32_t targetCombatId) {
    if (sim->g->run.phase != RunPhase::Combat) throw GameError("PlayCard: not in combat");
    CardPile hand = Player(sim, 1).PlayerCombatState().Hand();
    if (handIndex < 0 || handIndex >= hand.Count()) throw GameError("PlayCard: hand index out of range");
    CardModel card(sim, hand.refs()[static_cast<size_t>(handIndex)]);
    Creature target = targetCombatId < 0 ? Creature{} : CombatState(sim).GetCreature(targetCombatId);
    bool played = ActionExecutor::PlayCard(sim, card, target);
    AfterCombatAction(sim);
    return played;
}

namespace {

bool PotionTargetAllowed(PotionModel potion, Creature target) {
    Player owner = potion.Owner();
    TargetType type = potion.TargetType();
    if (type == TargetType::TargetedNoCreature || type == TargetType::AllEnemies ||
        type == TargetType::AllAllies || type == TargetType::RandomEnemy) {
        return !target;
    }
    if (!target || !target.IsAlive()) return false;
    switch (type) {
    case TargetType::Self: return target == owner.Creature();
    case TargetType::AnyPlayer: return target.IsPlayer();
    case TargetType::AnyEnemy: return target.IsEnemy();
    case TargetType::AnyAlly: return target.Side() == owner.Creature().Side() && target != owner.Creature();
    case TargetType::Osty: return false;
    default: return false;
    }
}

PotionModel PotionAt(Sim* sim, int32_t slotIndex) {
    Player player(sim, 1);
    PotionModel potion = player.GetPotionAtSlotIndex(slotIndex);
    if (!potion) throw GameError("UsePotion: potion slot is empty");
    if (!player.CanUseOrRemovePotions()) throw GameError("UsePotion: potions are disabled");
    if (potion.Usage() == PotionUsage::Automatic || !IsUsable(ModelKind::Potion, potion.DenseId())) {
        throw GameError("UsePotion: potion is not manually usable");
    }
    return potion;
}

}  // namespace

void UsePotion(Sim* sim, int32_t slotIndex, int32_t targetCombatId) {
    Player player(sim, 1);
    PotionModel potion = PotionAt(sim, slotIndex);
    bool inCombat = CombatManager(sim).IsInProgress();
    if (potion.Usage() == PotionUsage::CombatOnly && !inCombat) throw GameError("UsePotion: combat-only potion outside combat");
    if (!potion.PassesCustomUsabilityCheck()) throw GameError("UsePotion: potion fails custom usability check");
    Creature target;
    if (targetCombatId >= 0) target = CombatState(sim).GetCreature(targetCombatId);
    else if (!inCombat && (potion.TargetType() == TargetType::Self || potion.TargetType() == TargetType::AnyPlayer)) target = player.Creature();
    if (!PotionTargetAllowed(potion, target)) throw GameError("UsePotion: invalid potion target");
    PlayerChoiceContext context;
    potion.OnUseWrapper(context, target);
    // Vanilla ActionExecutor checks victory after every player-driven action,
    // including a potion. RunFlow directly executes its wrapper, so it must
    // perform this vanilla safe-point check for ordinary execution too.
    if (inCombat) CombatManager(sim).AfterPlayerAction();
    if (inCombat) AfterCombatAction(sim);
}

void DiscardPotion(Sim* sim, int32_t slotIndex) {
    Player player(sim, 1);
    PotionModel potion = player.GetPotionAtSlotIndex(slotIndex);
    if (!potion) throw GameError("DiscardPotion: potion slot is empty");
    if (!player.CanUseOrRemovePotions()) throw GameError("DiscardPotion: potions are disabled");
    PotionCmd::Discard(potion);
}

void EndTurn(Sim* sim) {
    if (sim->g->run.phase != RunPhase::Combat) throw GameError("EndTurn: not in combat");
    ActionExecutor::EndTurn(sim, Player(sim, 1).PlayerCombatState().TurnNumber());
    AfterCombatAction(sim);
}

const EventOptionList& EventOptions(Sim* sim) { return EventModel(sim).CurrentOptions(); }

void ChooseEventOption(Sim* sim, int32_t index) {
    if (sim->g->run.phase != RunPhase::Event) throw GameError("ChooseEventOption: not in an event");
    EventModel ev(sim);
    ev.ChooseOption(index);
    // The option may have started a rewards screen or ended the run.
    // IsProceed only selects the direct callback path in NEventRoom; Trial's
    // DoubleDown opens a modal and leaves the event unfinished. The finished
    // event's ordinary NEventRoom.Proceed is the actual map transition.
    if (sim->g->run.phase == RunPhase::Event && ev.IsFinished()) sim->g->run.phase = RunPhase::Map;
}

StaticVec<RestSiteOptionView, 8> RestSiteOptions(Sim* sim) {
    StaticVec<RestSiteOptionView, 8> out;
    Player player(sim, 1);
    for (RestSiteOptionId o : sim->g->run.rest_site.options) out.push_back({o, RestSite::IsEnabled(player, o)});
    return out;
}

bool CanProceedFromRestSite(Sim* sim) {
    const RestSiteState& r = sim->g->run.rest_site;
    return r.options.empty() || r.proceed_enabled;
}

bool ChooseRestSiteOption(Sim* sim, int32_t index) {
    if (sim->g->run.phase != RunPhase::RestSite) throw GameError("ChooseRestSiteOption: not at a rest site");
    StaticVec<RestSiteOptionView, 8> options = RestSiteOptions(sim);
    if (index < 0 || index >= static_cast<int32_t>(options.size()) || !options[static_cast<size_t>(index)].enabled) {
        throw GameError("ChooseRestSiteOption: no such enabled option");
    }
    bool success = RestSite::ChooseOption(sim, index);
    if (success) sim->g->run.rest_site.proceed_enabled = true;  // NRestSiteRoom.ShowProceedButton
    if (sim->g->run.phase == RunPhase::RestSite && sim->g->run.rest_site.options.empty()) sim->g->run.phase = RunPhase::Map;
    return success;
}

void ProceedFromRestSite(Sim* sim) {
    if (sim->g->run.phase != RunPhase::RestSite) throw GameError("ProceedFromRestSite: not at a rest site");
    if (!CanProceedFromRestSite(sim)) throw GameError("ProceedFromRestSite: the proceed button is not shown");
    sim->g->run.phase = RunPhase::Map;
}

const StaticVec<RelicId, 4>& TreasureRelics(Sim* sim) { return sim->g->run.treasure.current_relics; }

void PickTreasureRelic(Sim* sim, int32_t index) {
    if (sim->g->run.phase != RunPhase::Treasure) throw GameError("PickTreasureRelic: not in a treasure room");
    if (index < -1 || index >= static_cast<int32_t>(TreasureRelics(sim).size())) throw GameError("PickTreasureRelic: index out of range");
    TreasureRoom::PickRelic(sim, index);
    if (sim->g->run.phase == RunPhase::Treasure) sim->g->run.phase = RunPhase::Map;
}

StaticVec<Merchant::EntryRef, 16> ShopOptions(Sim* sim) {
    StaticVec<Merchant::EntryRef, 16> out;
    for (Merchant::EntryRef e : Merchant::AllEntries(sim)) {
        if (Merchant::IsStocked(sim, e)) out.push_back(e);
    }
    return out;
}

bool ShopOptionBuyable(Sim* sim, int32_t index) {
    StaticVec<Merchant::EntryRef, 16> options = ShopOptions(sim);
    return index >= 0 && index < static_cast<int32_t>(options.size()) && Merchant::EnoughGold(sim, options[static_cast<size_t>(index)]);
}

bool BuyShopOption(Sim* sim, int32_t index) {
    if (sim->g->run.phase != RunPhase::Shop) throw GameError("BuyShopOption: not in a shop");
    if (!ShopOptionBuyable(sim, index)) throw GameError("BuyShopOption: no such affordable entry");
    return Merchant::TryPurchase(sim, ShopOptions(sim)[static_cast<size_t>(index)]);
}

void LeaveShop(Sim* sim) {
    if (sim->g->run.phase != RunPhase::Shop) throw GameError("LeaveShop: not in a shop");
    sim->g->run.phase = RunPhase::Map;
}

StaticVec<Reward, 16> RewardOptions(Sim* sim) {
    StaticVec<Reward, 16> out;
    RewardsSet set(sim);
    for (size_t i = 0; i < set.Rewards().size(); ++i) out.push_back(set.Rewards()[i]);
    return out;
}

bool RewardTakable(Sim* sim, int32_t index) {
    StaticVec<Reward, 16> options = RewardOptions(sim);
    return index >= 0 && index < static_cast<int32_t>(options.size()) && !options[static_cast<size_t>(index)].SuccessfullySelected();
}

void TakeReward(Sim* sim, int32_t index) {
    if (sim->g->run.phase != RunPhase::Rewards) throw GameError("TakeReward: not on the rewards screen");
    if (!RewardTakable(sim, index)) throw GameError("TakeReward: no such reward to take");
    RewardsSet(sim).SelectReward(RewardOptions(sim)[static_cast<size_t>(index)]);
}

void ProceedFromRewards(Sim* sim) {
    if (sim->g->run.phase != RunPhase::Rewards) throw GameError("ProceedFromRewards: not on the rewards screen");
    auto& run = sim->g->run;
    if (!run.rooms.empty() && run.rooms.back() == RoomType::Boss) {
        // NRewardsScreen.OnProceedButtonPressed: the first of a double Boss
        // returns to this act's map. Every other terminal Boss votes to move
        // to the next act; a single player's ready vote completes immediately.
        const auto& map = run.map;
        const bool first_of_two = map.second_boss >= 0 && !run.visited_map_coords.empty()
            && run.visited_map_coords.back() == map.points[map.boss].coord;
        if (!first_of_two) {
            ++run.act_floor;  // ActChangeSynchronizer.MoveToNextAct, before room exit
            RunManager::EnterNextAct(sim);
            return;
        }
    }
    RewardsSet set(sim);
    if (set) set.Skip();  // SkipLocalRewardsSet, only while rewards are left
    if (InCombatRoom(sim)) ProceedFromTerminalCombat(sim);
    else sim->g->run.phase = RunPhase::Map;
}

}  // namespace sts2::RunFlow
