// Combat/CombatState.cs. There is at most one combat, so the handle carries
// no index: it is null when no combat exists.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/sim/sim.h"
#include "game/fwd.h"
#include "game/Models/AbstractModel.h"
#include "game/Models/signature_types.h"

namespace sts2 {

// The hook listener snapshot of IterateHookListeners (docs/design-engine.md §7).
// Ordinary combats stay inline; long summoning fights can accumulate more than
// 512 status cards, so spill only the excess into a local vector. This is not
// part of GameState and does not change POD cloning or snapshot semantics.
class HookListenerList {
public:
    class iterator {
    public:
        iterator(const HookListenerList* list, size_t index) : list_(list), index_(index) {}
        const AbstractModel& operator*() const { return (*list_)[index_]; }
        iterator& operator++() { ++index_; return *this; }
        bool operator!=(const iterator& other) const { return list_ != other.list_ || index_ != other.index_; }
    private:
        const HookListenerList* list_;
        size_t index_;
    };

    size_t size() const { return inline_.size() + overflow_.size(); }
    bool empty() const { return size() == 0; }
    iterator begin() const { return iterator(this, 0); }
    iterator end() const { return iterator(this, size()); }
    void push_back(AbstractModel model) {
        if (!inline_.full()) inline_.push_back(model);
        else overflow_.push_back(model);
    }
    AbstractModel& operator[](size_t i) { return i < inline_.size() ? inline_[i] : overflow_[i - inline_.size()]; }
    const AbstractModel& operator[](size_t i) const {
        return i < inline_.size() ? inline_[i] : overflow_[i - inline_.size()];
    }

private:
    StaticVec<AbstractModel, 512> inline_;
    std::vector<AbstractModel> overflow_;
};

class CombatState {
public:
    CombatState() = default;
    explicit CombatState(Sim* sim) : sim_(sim) {}

    explicit operator bool() const { return sim_ != nullptr && sim_->g->combat.exists; }
    friend bool operator==(const CombatState& a, const CombatState& b) { return bool(a) == bool(b); }
    Sim& sim() const { return *sim_; }
    CombatData& st() const { return sim_->g->combat; }

    sts2::RunState RunState() const;
    CreatureList Allies() const;
    CreatureList Enemies() const;
    CreatureList Creatures() const;         // allies then enemies
    CreatureList PlayerCreatures() const;
    StaticVec<Player, 4> Players() const;
    int32_t RoundNumber() const { return st().round_number; }
    void SetRoundNumber(int32_t v) { st().round_number = v; }
    CombatSide CurrentSide() const { return st().current_side; }
    void SetCurrentSide(CombatSide v) { st().current_side = v; }
    EncounterId Encounter() const { return st().encounter; }
    CreatureList EscapedCreatures() const;
    CreatureList CreaturesOnCurrentSide() const;
    CreatureList HittableEnemies() const;

    CardModel CreateCard(CardId canonicalCard, Player owner);
    CardModel CloneCard(CardModel mutableCard);
    void AddCard(CardModel card, Player owner);
    void RemoveCard(CardModel card);
    bool ContainsCard(CardModel card) const;
    void CaptureCardPlayPowerAmount(PowerModel power, CardModel card, int32_t amount);
    bool AddCardPlayPowerAmount(PowerModel power, CardModel card, int32_t amount);
    bool TakeCardPlayPowerAmount(PowerModel power, CardModel card, int32_t& amount);
    void ForgetCardPlayPowerCaptures(PowerModel power);
    void AddPlayer(Player player);
    // `init` sets the encounter's fields on the new monster before its HP is rolled.
    // CreateCreature(monster, side, slot): `monster` is a mutable monster from
    // MonsterModel::ToMutable; the creature takes over its pool slot.
    Creature CreateCreature(MonsterModel monster, CombatSide side, int8_t slot);
    void CreatureEscaped(Creature creature);
    void RemoveCreature(Creature creature, bool unattach = true);
    bool ContainsCreature(Creature creature) const;
    bool ContainsMonster(MonsterId monster) const;
    Creature GetCreature(int32_t combatId) const;
    CreatureList GetCreaturesOnSide(CombatSide side) const;
    CreatureList GetOpponentsOf(Creature creature) const;
    CreatureList GetTeammatesOf(Creature creature) const;

    // The snapshot IterateHookListeners builds when enumeration starts; call
    // Contains() on each item when its turn comes.
    HookListenerList IterateHookListeners() const;
    bool Contains(AbstractModel model) const;

    void SortEnemiesBySlotName();
    void SetEnemyIndex(Creature creature, int32_t index);
    void AddCreature(Creature creature);

private:
    friend ExecutionValue FreezeExecutionValue(CombatState);
    void AttachCreature(Creature creature);
    void AddCard(CardModel card);  // registers the card in the combat (_allCards)

    Sim* sim_ = nullptr;
};

}  // namespace sts2
