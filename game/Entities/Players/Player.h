// Entities/Players/Player.cs (single player: the index is always 1)
#pragma once

#include <cstdint>

#include "core/rng.h"
#include "engine/sim/sim.h"
#include "game/fwd.h"
#include "game/Models/signature_types.h"

namespace sts2 {

class Player {
public:
    Player() = default;
    Player(Sim* sim, uint8_t index) : sim_(sim), index_(index) {}

    explicit operator bool() const { return index_ != 0; }
    friend bool operator==(const Player& a, const Player& b) { return a.index_ == b.index_; }
    uint8_t Index() const { return index_; }
    Sim& sim() const { return *sim_; }
    PlayerData& st() const { return sim_->g->run.player; }

    CharacterId Character() const { return st().character; }
    sts2::Creature Creature() const;
    uint64_t NetId() const { return st().net_id; }
    PlayerRngSet& PlayerRng() const { return st().rng; }
    sts2::RunState RunState() const;
    bool IsActiveForHooks() const { return st().active_for_hooks; }
    void DeactivateHooks() const { st().active_for_hooks = false; }
    void ActivateHooks() const { st().active_for_hooks = true; }
    sts2::PlayerCombatState PlayerCombatState() const;  // null outside combat
    StaticVec<RelicModel, kMaxRelics> Relics() const;
    StaticVec<PotionModel, kMaxPotionSlots> PotionSlots() const;  // null handles for empty slots
    StaticVec<PotionModel, kMaxPotionSlots> Potions() const;      // non-empty slots
    int32_t MaxPotionCount() const { return st().max_potion_slots; }
    int32_t Gold() const { return st().gold; }
    void SetGold(int32_t gold);
    bool HasOpenPotionSlots() const;
    bool CanUseOrRemovePotions() const;
    void SetCanUseOrRemovePotions(bool value) { st().can_use_or_remove_potions = value; }
    CardPile Deck() const;
    int32_t MaxEnergy() const { return st().max_energy; }
    void SetMaxEnergy(int32_t v) { st().max_energy = v; }
    int32_t BaseOrbSlotCount() const { return st().base_orb_slots; }

    void AddRelicInternal(RelicModel relic, int32_t index = -1, bool silent = false);
    // AddPotionInternal(potion, slotIndex, silent): PotionProcureResult.
    struct PotionProcureResult {
        bool success = false;
        PotionProcureFailureReason failureReason = PotionProcureFailureReason::None;
    };
    PotionProcureResult AddPotionInternal(PotionModel potion, int32_t slotIndex = -1, bool silent = false);
    void RemoveRelicInternal(RelicModel relic, bool silent = false);
    void MeltRelicInternal(RelicModel relic);
    RelicModel GetRelic(RelicId id) const;
    int32_t GetPotionSlotIndex(PotionModel potion) const;
    PotionModel GetPotionAtSlotIndex(int32_t index) const;
    void AddToMaxPotionCount(int32_t increase);
    void SubtractFromMaxPotionCount(int32_t decrease);
    void DiscardPotionInternal(PotionModel potion, bool silent = false);
    void RemoveUsedPotionInternal(PotionModel potion);

    void ResetCombatState();
    void PopulateCombatState(Rng& rng, sts2::CombatState state);
    void ReviveBeforeCombatEnd();
    void AfterCombatEnd();
    void OnSideSwitch();

private:
    void SetMaxPotionCountInternal(int32_t count);
    Sim* sim_ = nullptr;
    uint8_t index_ = 0;
};

}  // namespace sts2
