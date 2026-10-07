// Rewards/Reward.cs and its subclasses (GoldReward, PotionReward,
// RelicReward, CardReward), and Rewards/RewardsSet.cs.
//
// A reward is a RewardState in GameState::rewards; `type` selects the
// subclass. `Reward` is the handle (like CardModel for cards). The C#
// constructors are the static New* functions: they allocate the reward.
#pragma once

#include <cstdint>

#include "engine/sim/sim.h"
#include "game/fwd.h"
#include "game/Models/signature_types.h"

namespace sts2 {

class Reward {
public:
    Reward() = default;
    Reward(Sim* sim, RewardRef index) : sim_(sim), index_(index) {}

    explicit operator bool() const { return index_ != 0; }
    friend bool operator==(const Reward& a, const Reward& b) { return a.index_ == b.index_; }
    Sim& sim() const { return *sim_; }
    RewardRef Index() const { return index_; }
    RewardState& st() const { return sim_->g->rewards[index_]; }

    // --- constructors
    static Reward NewGoldReward(Player player, int32_t min, int32_t max, bool wasGoldStolenBack = false);
    static Reward NewGoldReward(Player player, int32_t amount, bool wasGoldStolenBack = false);
    static Reward NewPotionReward(Player player);
    static Reward NewPotionReward(Player player, PotionModel potion);
    static Reward NewRelicReward(Player player);
    static Reward NewRelicReward(Player player, RelicRarity rarity);
    static Reward NewRelicReward(Player player, RelicModel relic);
    static Reward NewCardRemovalReward(Player player);
    static Reward NewCardReward(Player player, CardCreationOptionsState options, int32_t cardCount);
    static Reward NewCardReward(Player player, const CardList& cards);

    static Reward NewSpecialCardReward(Player player, CardModel card);

    // CrystalSphereItem.SetRng: use the active event RNG while populating this reward.
    Reward& UseEventRng() { st().use_event_rng = true; return *this; }

    // --- Reward
    RewardType Type() const { return st().type; }
    sts2::Player Player() const;
    int32_t RewardsSetIndex() const;
    bool IsPopulated() const;
    bool SuccessfullySelected() const { return st().successfully_selected; }
    void Populate() const;
    void OnSkipped() const;
    bool SelectUnsynchronized() const;

    // --- GoldReward
    int32_t Amount() const { return st().gold_amount; }
    // --- PotionReward
    PotionModel Potion() const;
    // --- RelicReward
    RelicModel Relic() const;
    // --- CardReward
    CardList Cards() const;
    bool CanSkip() const { return st().can_skip; }
    bool CanReroll() const { return st().can_reroll; }
    void Reroll() const;
    // Player.RelicObtained, while subscribed (CardReward.OnRelicObtained).
    void OnRelicObtained(RelicModel relic) const;

private:
    bool OnSelect() const;
    bool CardRewardOnSelect() const;

    Sim* sim_ = nullptr;
    RewardRef index_ = 0;
};

// List<Reward>: the rewards of the set being offered (RewardsSet.Rewards).
class RewardList {
public:
    RewardList() = default;
    explicit RewardList(Sim* sim) : sim_(sim) {}
    size_t size() const;
    Reward operator[](size_t i) const;
    void Add(Reward reward) const;
    bool contains(Reward reward) const;

private:
    friend ExecutionValue FreezeExecutionValue(const RewardList&);
    Sim* sim_ = nullptr;
};

// Rewards/RewardsSet.cs with the single-player RewardsSetSynchronizer.
class RewardsSet {
public:
    RewardsSet() = default;
    explicit RewardsSet(Sim* sim) : sim_(sim) {}
    explicit operator bool() const { return sim_ != nullptr && sim_->g->run.rewards.active; }
    RewardsSetState& st() const { return sim_->g->run.rewards; }

    RewardList Rewards() const { return RewardList(sim_); }
    sts2::Player Player() const;

    // new RewardsSet(player).WithRewardsFromRoom(room) / .EmptyForRoom(room) /
    // .WithCustomRewards(rewards). Starts a new set (replacing a finished one).
    static RewardsSet NewForRoom(Sim* sim, RoomType room);
    static RewardsSet NewEmptyForRoom(Sim* sim, RoomType room);
    static RewardsSet NewCustom(Sim* sim, const StaticVec<Reward, 8>& rewards);

    void GenerateWithoutOffering() const;
    // Offer: generates the rewards and shows the screen (RunPhase::Rewards).
    // Returns false if nothing is offered (the C# method returns at once).
    bool Offer() const;
    bool AllRewardsSuccessfullySelected() const;

    // The player's choices on the screen.
    bool SelectReward(Reward reward) const;  // RewardsSetSynchronizer.SelectLocalReward
    void Skip() const;                       // SkipLocalRewardsSet (proceed)
    bool IsCompleted() const { return !st().active; }

private:
    friend ExecutionValue FreezeExecutionValue(const RewardsSet&);
    void Complete() const;
    Sim* sim_ = nullptr;
};

}  // namespace sts2
