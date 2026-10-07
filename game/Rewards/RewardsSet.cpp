// Rewards/RewardsSet.cs with the single-player parts of
// Multiplayer/Game/RewardsSetSynchronizer.cs.
#include <cmath>

#include "core/dotnet_sort.h"
#include "game/All.h"
#include "game/Hooks/Hook.h"
#include "game/Odds/PotionRewardOdds.h"
#include "game/Runs/CardCreationOptions.h"

namespace sts2 {

namespace {

// RollForPotionAndAddTo
void RollForPotionAndAddTo(StaticVec<Reward, 8>& rewards, Player player, RoomType roomType) {
    if (PotionRewardOdds::Roll(player, roomType)) rewards.push_back(Reward::NewPotionReward(player));
}

// (int)Math.Round((float)amount * GoldProportion)
int32_t ScaledGold(int32_t amount, float proportion) {
    return static_cast<int32_t>(std::nearbyint(static_cast<double>(static_cast<float>(amount) * proportion)));
}

int32_t MinGoldReward(const Sim& sim, EncounterId e) {
    return sim.g->run.has_ascension(AscensionLevel::Poverty) ? data(e).min_gold_poverty : data(e).min_gold;
}
int32_t MaxGoldReward(const Sim& sim, EncounterId e) {
    return sim.g->run.has_ascension(AscensionLevel::Poverty) ? data(e).max_gold_poverty : data(e).max_gold;
}

CardCreationOptionsState CombatCardOptions(Player player, RoomType roomType) {
    CardCreationOptionsState o = CardCreationOptions::ForRoom(player, roomType);
    return CardCreationOptions::WithFlags(o, CardCreationFlags::IsFromCombat);
}

// GenerateRewardsFor(player, room)
StaticVec<Reward, 8> GenerateRewardsFor(Sim* sim, Player player, RoomType roomType) {
    StaticVec<Reward, 8> list;
    bool combat = roomType == RoomType::Monster || roomType == RoomType::Elite || roomType == RoomType::Boss;
    if (!combat) {
        if (roomType != RoomType::Treasure) throw GameError("Tried to generate a reward for invalid room type");
        return list;
    }
    EncounterId encounter = sim->g->combat.encounter;
    float goldProportion = sim->g->combat.gold_proportion;
    switch (roomType) {
    case RoomType::Monster:
        if (goldProportion > 0.0f) {
            list.push_back(Reward::NewGoldReward(player, ScaledGold(MinGoldReward(*sim, encounter), goldProportion),
                                                 ScaledGold(MaxGoldReward(*sim, encounter), goldProportion)));
        }
        RollForPotionAndAddTo(list, player, roomType);
        list.push_back(Reward::NewCardReward(player, CombatCardOptions(player, roomType), 3));
        break;
    case RoomType::Elite:
        list.push_back(Reward::NewGoldReward(player, MinGoldReward(*sim, encounter), MaxGoldReward(*sim, encounter)));
        RollForPotionAndAddTo(list, player, roomType);
        list.push_back(Reward::NewCardReward(player, CombatCardOptions(player, roomType), 3));
        list.push_back(Reward::NewRelicReward(player));
        break;
    case RoomType::Boss:
        list.push_back(Reward::NewGoldReward(player, MinGoldReward(*sim, encounter), MaxGoldReward(*sim, encounter)));
        RollForPotionAndAddTo(list, player, roomType);
        list.push_back(Reward::NewCardReward(player, CombatCardOptions(player, roomType), 3));
        break;
    default: break;
    }
    return list;
}

}  // namespace

Player RewardsSet::Player() const { return sts2::Player(sim_, 1); }

// new RewardsSet(player).WithRewardsFromRoom(room). TryGenerateTutorialRewards
// needs NumberOfRuns == 0 (never on the fully unlocked profile). CombatRoom
// ExtraRewards are not ported (no relic adds them yet).
RewardsSet RewardsSet::NewForRoom(Sim* sim, RoomType room) {
    RewardsSet set(sim);
    set.st() = RewardsSetState{};
    set.st().room = room;
    RunData& run = sim->g->run;
    if (room == RoomType::Boss && run.current_act_index >= static_cast<int32_t>(run.acts.size()) - 1) return set;
    StaticVec<Reward, 8> rewards = GenerateRewardsFor(sim, set.Player(), room);
    for (Reward r : rewards) set.Rewards().Add(r);
    if (room == RoomType::Monster || room == RoomType::Elite || room == RoomType::Boss)
        for (RewardRef ref : run.combat_extra_rewards) set.Rewards().Add(Reward(sim, ref));
    return set;
}

RewardsSet RewardsSet::NewEmptyForRoom(Sim* sim, RoomType room) {
    RewardsSet set(sim);
    set.st() = RewardsSetState{};
    set.st().room = room;
    set.st().allow_empty = true;
    return set;
}

RewardsSet RewardsSet::NewCustom(Sim* sim, const StaticVec<Reward, 8>& rewards) {
    RewardsSet set(sim);
    set.st() = RewardsSetState{};
    for (Reward r : rewards) set.Rewards().Add(r);
    return set;
}

void RewardsSet::GenerateWithoutOffering() const {
    if (st().generated) return;
    auto second = st().rewards;  // Rewards.ToList()
    for (size_t i = 0; i < Rewards().size(); ++i) Rewards()[i].Populate();
    ModelList modifiers = Hook::ModifyRewards(Player().RunState(), Player(), Rewards(),
                                              AbstractRoom(sim_, st().room));
    for (size_t i = 0; i < Rewards().size(); ++i) {  // Rewards.Except(second)
        Reward item = Rewards()[i];
        if (second.contains(item.Index())) continue;
        if (!item.IsPopulated()) item.Populate();
    }
    Hook::AfterModifyingRewards(Player().RunState(), modifiers);
    // Rewards.Sort(x.RewardsSetIndex.CompareTo(y.RewardsSetIndex)): .NET introsort.
    Sim* sim = sim_;
    auto& refs = st().rewards;
    dotnet_sort(std::span<RewardRef>(refs.data(), refs.size()), [sim](RewardRef a, RewardRef b) {
        int32_t x = Reward(sim, a).RewardsSetIndex(), y = Reward(sim, b).RewardsSetIndex();
        return x < y ? -1 : (x > y ? 1 : 0);
    });
    st().generated = true;
}

bool RewardsSet::AllRewardsSuccessfullySelected() const {
    for (size_t i = 0; i < Rewards().size(); ++i) {
        if (!Rewards()[i].SuccessfullySelected()) return false;
    }
    return true;
}

// Offer: in the game the task waits until the set is completed on the
// rewards screen; here the run waits in RunPhase::Rewards. Returns whether the
// screen is shown: not for a dead player, nor for an empty set outside a
// combat room (unless empty sets are allowed). BeginRewardsSet completes a set
// with nothing left to take at once; the screen still shows (proceed).
bool RewardsSet::Offer() const {
    if (Player().Creature().IsDead()) return false;
    GenerateWithoutOffering();
    RoomType room = st().room;
    bool combatRoom = room == RoomType::Monster || room == RoomType::Elite || room == RoomType::Boss;
    st().active = true;  // BeginRewardsSet
    if (AllRewardsSuccessfullySelected()) Complete();
    if (Rewards().size() == 0 && !combatRoom && !st().allow_empty) return false;
    return true;
}

bool RewardsSet::SelectReward(Reward reward) const {
    if (!st().active) throw GameError("Tried to sync reward for local player, but they are not currently viewing any reward set!");
    bool result = reward.SelectUnsynchronized();
    if (AllRewardsSuccessfullySelected()) Complete();
    return result;
}

void RewardsSet::Skip() const {
    if (!st().active) throw GameError("Tried to skip reward set, but they are not currently viewing any reward set!");
    for (size_t i = 0; i < Rewards().size(); ++i) {
        Reward reward = Rewards()[i];
        if (!reward.SuccessfullySelected()) reward.OnSkipped();
    }
    Complete();
}

void RewardsSet::Complete() const { st().active = false; }

}  // namespace sts2
