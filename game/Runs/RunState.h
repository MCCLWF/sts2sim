// Runs/RunState.cs (the parts the ported game code reads)
#pragma once

#include <cstdint>

#include "core/rng.h"
#include "engine/sim/sim.h"
#include "game/fwd.h"
#include "game/Combat/CombatState.h"
#include "game/Models/signature_types.h"

namespace sts2 {

class RunState {
public:
    RunState() = default;
    explicit RunState(Sim* sim) : sim_(sim) {}

    explicit operator bool() const { return sim_ != nullptr; }
    Sim& sim() const { return *sim_; }
    RunData& st() const { return sim_->g->run; }

    RunRngSet& Rng() const { return st().rng; }
    StaticVec<Player, 4> Players() const;
    int32_t CurrentActIndex() const { return st().current_act_index; }
    ActId Act() const { return st().acts[static_cast<size_t>(st().current_act_index)].id; }
    int32_t AscensionLevel() const { return st().ascension; }
    // MapCoord of the current map point; col/row are -1 before the first move.
    int32_t CurrentMapCol() const;
    int32_t CurrentMapRow() const;
    bool AddVisitedMapCoord(MapCoord coord);
    int32_t ActFloor() const { return st().act_floor; }
    int32_t CurrentRoomCount() const { return static_cast<int32_t>(st().rooms.size()); }

    int32_t TotalFloor() const { return st().total_floor; }

    CardModel CreateCard(CardId canonicalCard, Player owner);
    CardModel CloneCard(CardModel mutableCard);
    void AddCard(CardModel card, Player owner);
    void AddCard(CardModel card);
    void RemoveCard(CardModel card);
    bool ContainsCard(CardModel card) const;

    // IRunState.IterateHookListeners(ICombatState? childCombatState): the
    // snapshot, as CombatState::IterateHookListeners (check Contains per item).
    // Only the run-level part (deck cards and their enchantments; relics,
    // potions and modifiers when there is no combat): the child combat's
    // listeners follow, snapshotted when the enumeration reaches them
    // (Hook.cpp, HookListenerRange).
    HookListenerList IterateHookListeners(sts2::CombatState childCombatState) const;
    bool Contains(AbstractModel model) const;  // RunState.Contains

private:
    Sim* sim_ = nullptr;
};

}  // namespace sts2
