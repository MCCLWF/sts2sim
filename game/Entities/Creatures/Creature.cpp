// Entities/Creatures/Creature.cs
#include "game/Entities/Creatures/Creature.h"

#include <string>

#include "game/All.h"
#include "game/Hooks/Hook.h"
#include "game/Models/EncounterModel.h"

namespace sts2 {

namespace {
// The Block / CurrentHp setters: values must not be negative.
void set_block(CreatureState& s, int32_t v) {
    if (v < 0) throw GameError("Block must be positive");
    s.block = v;
}
void set_current_hp(CreatureState& s, int32_t v) {
    if (v < 0) throw GameError("Current HP must be positive");
    s.current_hp = v;
}
}  // namespace

MonsterModel Creature::Monster() const { return IsMonster() ? MonsterModel(sim_, index_) : MonsterModel{}; }
sts2::Player Creature::Player() const { return IsPlayer() ? sts2::Player(sim_, st().player) : sts2::Player{}; }
sts2::CombatState Creature::CombatState() const { return st().attached ? sts2::CombatState(sim_) : sts2::CombatState{}; }
sts2::Player Creature::PetOwner() const { return st().pet_owner ? sts2::Player(sim_, st().pet_owner) : sts2::Player{}; }

std::string_view Creature::SlotName() const {
    if (st().slot < 0) return {};
    return EncounterModel(sim_).Slots()[static_cast<size_t>(st().slot)];
}

bool Creature::IsPrimaryEnemy() const {
    if (Side() != CombatSide::Enemy) return false;
    return !IsSecondaryEnemy();
}

bool Creature::IsSecondaryEnemy() const {
    if (Side() != CombatSide::Enemy) return false;
    for (PowerModel p : Powers()) {
        if (p.OwnerIsSecondaryEnemy()) return true;
    }
    return false;
}

bool Creature::IsHittable() const {
    if (IsDead()) return false;
    if (!Hook::ShouldAllowHitting(CombatState(), *this)) return false;
    return true;
}

bool Creature::CanReceivePowers() const {
    if (!CombatState()) return false;
    if (!Hook::ShouldAllowHitting(CombatState(), *this)) return false;
    return true;
}

bool Creature::IsStunned() const { return IsMonster() && Monster().NextMove().Id() == MonsterModel::stunnedMoveId; }

StaticVec<PowerModel, kMaxPowersPerCreature> Creature::Powers() const {
    StaticVec<PowerModel, kMaxPowersPerCreature> out;
    for (PowerRef p : st().powers) out.push_back(PowerModel(sim_, p));
    return out;
}

// SetUniqueMonsterHpValue: HashSet<int> of [min, max], minus the other
// creatures' MaxHp (ExceptWith), then NextItem over the set in its
// enumeration order (insertion order with the removed values left out).
void Creature::SetUniqueMonsterHpValue(const CreatureList& creaturesOnSide, Rng& rng) {
    if (!IsMonster()) throw GameError("Can't set unique monster HP value for a player.");
    int32_t minInitialHp = Monster().MinInitialHp();
    int32_t num = Monster().MaxInitialHp() + 1;
    StaticVec<int32_t, 256> hashSet;
    for (int32_t v = minInitialHp; v < num; ++v) hashSet.push_back(v);
    for (Creature e : creaturesOnSide) {
        if (e == *this) continue;
        hashSet.remove(e.MaxHp());
    }
    int32_t hp;
    if (hashSet.size() <= 0) {
        hp = rng.NextInt(minInitialHp, num);
    } else {
        hp = rng.NextItem(hashSet);
    }
    st().max_hp = hp;
    st().current_hp = hp;
    st().monster_max_hp_before_modification = hp;
}

void Creature::ScaleMonsterHpForMultiplayer(EncounterId encounter, int32_t playerCount, int32_t actIndex) {
    if (playerCount != 1) {
        SetMaxHpInternal(ScaleHpForMultiplayer(Dec(MaxHp()), encounter, playerCount, actIndex));
        SetCurrentHpInternal(Dec(MaxHp()));
    }
}

void Creature::AfterAddedToRoom() {
    if (Side() == CombatSide::Enemy) Monster().AfterAddedToRoom();
}

Dec Creature::DamageBlockInternal(Dec amount, ValueProp props) {
    Dec num = has_flag(props, ValueProp::Unblockable) ? Dec(0) : dec_min(Dec(Block()), amount);
    set_block(st(), Block() - num.to_int());
    return num;
}

DamageResult Creature::LoseHpInternal(Dec amount, ValueProp props) {
    bool flag = CurrentHp() > 0 && amount >= Dec(CurrentHp());
    int32_t currentHp = CurrentHp();
    int32_t num = dec_min(dec_max(amount, Dec(0)), Dec(999999999)).to_int();  // Math.Clamp
    set_current_hp(st(), std::max(CurrentHp() - num, 0));
    DamageResult r(*this, props);
    r.UnblockedDamage = currentHp - CurrentHp();
    r.WasTargetKilled = flag;
    r.OverkillDamage = flag ? std::max(num - currentHp, 0) : 0;
    return r;
}

void Creature::GainBlockInternal(Dec amount) {
    if (amount < Dec(0)) throw GameError("amount must be positive. Use LoseBlock for block loss.");
    set_block(st(), dec_min(Dec(Block()) + amount, Dec(999999999)).to_int());
}

void Creature::LoseBlockInternal(Dec amount) {
    if (amount < Dec(0)) throw GameError("amount must be positive. Use GainBlock for block gain.");
    set_block(st(), dec_max(Dec(Block()) - amount, Dec(0)).to_int());
}

void Creature::HealInternal(Dec amount) {
    bool isDead = IsDead();
    SetCurrentHpInternal(Dec(CurrentHp()) + amount);
    if (isDead && !IsDead() && IsPlayer()) Player().ActivateHooks();
}

void Creature::SetCurrentHpInternal(Dec amount) { set_current_hp(st(), dec_min(amount, Dec(MaxHp())).to_int()); }

void Creature::SetMaxHpInternal(Dec amount) {
    if (amount < Dec(0)) throw GameError("amount must be non-negative.");
    st().max_hp = std::min(amount.to_int(), 999999999);
    set_current_hp(st(), std::min(CurrentHp(), MaxHp()));
}

void Creature::Reset() {
    RemoveAllPowersInternalExcept();
    set_block(st(), 0);
}

// Died?.Invoke(this): the only gameplay subscriber is PlayerCombatState.OnPetDied.
void Creature::InvokeDiedEvent() {
    if (!IsPet()) return;
    sts2::Player owner = PetOwner();
    PlayerCombatState pcs = owner.PlayerCombatState();
    if (!pcs) return;
    pcs.OnPetDied(*this);
}

void Creature::StunInternal(MoveFn stunMove, std::string_view nextMoveId) {
    if (!IsMonster()) throw GameError("Can't stun a player.");
    if (CombatState() && !IsDead()) {
        std::string next(nextMoveId);
        if (next.empty()) next = std::string(Monster().LastLoggedMoveId());
        Monster().SetMoveImmediateStunned(stunMove, next);
    }
}

void Creature::PrepareForNextTurn(const CreatureList& targets, bool rollNewMove) {
    if (rollNewMove && Monster().HasMoveStateMachine()) Monster().RollMove(targets);
}

bool Creature::HasPower(PowerId id) const {
    for (PowerRef p : st().powers) {
        if (sim_->g->powers[p].id == id) return true;
    }
    return false;
}

PowerModel Creature::GetPower(PowerId id) const {
    for (PowerRef p : st().powers) {
        if (sim_->g->powers[p].id == id) return PowerModel(sim_, p);
    }
    return {};
}

StaticVec<PowerModel, kMaxPowersPerCreature> Creature::GetPowerInstances(PowerId id) const {
    StaticVec<PowerModel, kMaxPowersPerCreature> out;
    for (PowerRef p : st().powers) {
        if (sim_->g->powers[p].id == id) out.push_back(PowerModel(sim_, p));
    }
    return out;
}

int32_t Creature::GetPowerAmount(PowerId id) const {
    PowerModel p = GetPower(id);
    return p ? p.Amount() : 0;
}

void Creature::ApplyPowerInternal(PowerModel power) {
    if (power.Owner() != *this) throw GameError("ONLY CALL THIS FROM PowerModel.ApplyInternal!");
    if (power.InstanceType() == PowerInstanceType::None && HasPower(power.PowerId_())) {
        throw GameError("Trying to add multiple instances of a non-instanced power to a creature.");
    }
    st().powers.push_back(power.Index());
    if (sim_->execution_trace)
        sim_->execution_trace->RecordPower({ExecutionPowerEventKind::Attached,
            power.Index(), Index(), static_cast<uint32_t>(CombatId()), static_cast<uint16_t>(power.PowerId_()),
            power.Amount(), power.Amount(), true});
}

void Creature::InvokePowerModified(PowerModel power, int32_t change, bool silent) {}  // UI events only

void Creature::RemovePowerInternal(PowerModel power) {
    if (power.Owner() != *this) throw GameError("ONLY CALL THIS FROM PowerModel.RemoveInternal!");
    if (st().powers.contains(power.Index()) && sim_->execution_trace)
        sim_->execution_trace->RecordPower({ExecutionPowerEventKind::Removed,
            power.Index(), Index(), static_cast<uint32_t>(CombatId()), static_cast<uint16_t>(power.PowerId_()),
            power.Amount(), power.Amount(), true});
    st().powers.remove(power.Index());
}

StaticVec<PowerModel, kMaxPowersPerCreature> Creature::RemoveAllPowersInternalExcept(
    const StaticVec<PowerModel, kMaxPowersPerCreature>* except) {
    StaticVec<PowerModel, kMaxPowersPerCreature> list;  // _powers.Except(except).ToList()
    for (PowerModel p : Powers()) {
        bool excluded = false;
        if (except != nullptr) {
            for (PowerModel e : *except) excluded |= e == p;
        }
        bool dup = false;
        for (PowerModel q : list) dup |= q == p;
        if (!excluded && !dup) list.push_back(p);
    }
    for (PowerModel item : list) item.RemoveInternal();
    return list;
}

StaticVec<PowerModel, kMaxPowersPerCreature> Creature::RemoveAllPowersAfterDeath() {
    // Except(_powers.Where(...)) is lazy: the Where runs inside Except, before
    // any power is removed, so evaluating it up front is equivalent.
    StaticVec<PowerModel, kMaxPowersPerCreature> keep;
    for (PowerModel p : Powers()) {
        if (!p.ShouldPowerBeRemovedAfterOwnerDeath() || !Hook::ShouldPowerBeRemovedOnDeath(p)) keep.push_back(p);
    }
    return RemoveAllPowersInternalExcept(&keep);
}

void Creature::BeforeTurnStart(CombatSide side) {
    for (PowerModel p : Powers()) p.SetAmountOnTurnStart(p.Amount());
}

void Creature::AfterTurnStart(CombatSide side) {
    if (side == CombatSide::Player) {
        sts2::Player player = Player();
        if (player && player.PlayerCombatState() && player.PlayerCombatState().TurnNumber() == 1) return;
    }
    ClearBlock();
}

void Creature::OnSideSwitch() {
    if (IsPlayer()) {
        Player().OnSideSwitch();
    } else {
        Monster().OnSideSwitch();
    }
}

void Creature::TakeTurn() {
    if (!IsMonster() || Side() != CombatSide::Enemy) throw GameError("Only enemy monsters can take automated turns.");
    if (!Monster().SpawnedThisTurn()) Monster().PerformMove();
}

void Creature::ClearBlock() {
    AbstractModel preventer;
    if (Hook::ShouldClearBlock(CombatState(), *this, preventer)) {
        set_block(st(), 0);
    } else {
        Hook::AfterPreventingBlockClear(CombatState(), preventer, *this);
    }
}

double Creature::GetHpPercentRemaining() const {
    return static_cast<double>(st().current_hp) / static_cast<double>(st().max_hp);
}

Dec Creature::ScaleHpForMultiplayer(Dec hp, EncounterId encounter, int32_t playerCount, int32_t actIndex) {
    if (playerCount <= 1) return hp;
    throw GameError("multiplayer HP scaling is not supported (single player only)");
}

}  // namespace sts2
