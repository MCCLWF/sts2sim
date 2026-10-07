// Entities/Creatures/Creature.cs
#pragma once

#include <cstdint>
#include <string_view>

#include "core/dec.h"
#include "core/rng.h"
#include "engine/sim/sim.h"
#include "game/fwd.h"
#include "game/Models/signature_types.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

class Creature {
public:
    Creature() = default;
    Creature(Sim* sim, CreatureRef index) : sim_(sim), index_(index) {}

    explicit operator bool() const { return index_ != 0; }
    friend bool operator==(const Creature& a, const Creature& b) { return a.index_ == b.index_; }
    CreatureRef Index() const { return index_; }
    Sim& sim() const { return *sim_; }
    CreatureState& st() const { return sim_->g->creatures[index_]; }

    // --- state
    int32_t Block() const { return st().block; }
    int32_t CurrentHp() const { return st().current_hp; }
    int32_t MaxHp() const { return st().max_hp; }
    sts2::HpDisplay HpDisplay() const { return st().hp_display; }
    void SetHpDisplay(sts2::HpDisplay display) const { st().hp_display = display; }
    int32_t MonsterMaxHpBeforeModification() const { return st().monster_max_hp_before_modification; }  // -1 = null
    int32_t CombatId() const { return st().combat_id; }  // -1 = null
    CombatSide Side() const { return st().side; }
    MonsterModel Monster() const;  // null for a player's creature
    sts2::Player Player() const;   // null for a monster
    sts2::CombatState CombatState() const;  // null when not attached to a combat
    std::string_view SlotName() const;      // "" = null
    bool HasSlotName() const { return st().slot >= 0; }
    bool IsMonster() const { return st().player == 0; }
    bool IsPlayer() const { return st().player != 0; }
    sts2::Player PetOwner() const;
    bool IsPet() const { return st().pet_owner != 0; }
    bool IsAlive() const { return CurrentHp() > 0; }
    bool IsDead() const { return !IsAlive(); }
    bool IsEnemy() const { return Side() == CombatSide::Enemy; }
    bool IsPrimaryEnemy() const;
    bool IsSecondaryEnemy() const;
    bool IsHittable() const;
    bool CanReceivePowers() const;
    bool IsStunned() const;
    StaticVec<PowerModel, kMaxPowersPerCreature> Powers() const;  // snapshot, C# order

    // --- setup
    void SetUniqueMonsterHpValue(const CreatureList& creaturesOnSide, Rng& rng);
    void ScaleMonsterHpForMultiplayer(EncounterId encounter, int32_t playerCount, int32_t actIndex);
    void AfterAddedToRoom();

    // --- low-level state changes (the *Internal methods; commands call these)
    Dec DamageBlockInternal(Dec amount, ValueProp props);
    DamageResult LoseHpInternal(Dec amount, ValueProp props);
    void GainBlockInternal(Dec amount);
    void LoseBlockInternal(Dec amount);
    void HealInternal(Dec amount);
    void SetCurrentHpInternal(Dec amount);
    void SetMaxHpInternal(Dec amount);
    void Reset();
    void InvokeDiedEvent();
    // `stunMove`: the logic of the STUNNED move (a SIM_MOVE callback, null =
    // do nothing); `nextMoveId`: "" = the last logged move.
    void StunInternal(MoveFn stunMove, std::string_view nextMoveId);
    void PrepareForNextTurn(const CreatureList& targets, bool rollNewMove = true);

    // --- powers
    bool HasPower(PowerId id) const;
    PowerModel GetPower(PowerId id) const;  // first instance, or null
    StaticVec<PowerModel, kMaxPowersPerCreature> GetPowerInstances(PowerId id) const;
    int32_t GetPowerAmount(PowerId id) const;
    void ApplyPowerInternal(PowerModel power);
    void InvokePowerModified(PowerModel power, int32_t change, bool silent);
    void RemovePowerInternal(PowerModel power);
    StaticVec<PowerModel, kMaxPowersPerCreature> RemoveAllPowersInternalExcept(
        const StaticVec<PowerModel, kMaxPowersPerCreature>* except = nullptr);
    StaticVec<PowerModel, kMaxPowersPerCreature> RemoveAllPowersAfterDeath();

    // --- turn flow
    void BeforeTurnStart(CombatSide side);
    void AfterTurnStart(CombatSide side);
    void OnSideSwitch();
    void TakeTurn();

    double GetHpPercentRemaining() const;
    static Dec ScaleHpForMultiplayer(Dec hp, EncounterId encounter, int32_t playerCount, int32_t actIndex);

private:
    void ClearBlock();

    Sim* sim_ = nullptr;
    CreatureRef index_ = 0;
};

}  // namespace sts2
