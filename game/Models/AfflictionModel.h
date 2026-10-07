// Models/AfflictionModel.cs. An affliction is an independent mutable object.
// Attached objects keep a card reference; ToMutable objects are pending,
// writable objects with their own identity until CardCmd.Afflict attaches them.
#pragma once

#include <cstdint>

#include "game/Models/AbstractModel.h"
#include "engine/sim/alloc.h"

namespace sts2 {

class AfflictionModel : public AbstractModel {
public:
    static constexpr uint32_t kHandle = 0x80000000;

    AfflictionModel() = default;
    AfflictionModel(Sim* sim, CardRef card) : AbstractModel(sim, ModelKind::Affliction,
        sim && card && sim->g->cards.live(card) && sim->g->cards[card].affliction_instance
            ? static_cast<uint32_t>(kHandle | sim->g->cards[card].affliction_instance) : 0) {}
    AfflictionModel(Sim* sim, AfflictionRef ref, bool) : AbstractModel(sim, ModelKind::Affliction,
        static_cast<uint32_t>(kHandle | ref)) {}
    // ModelDb.Affliction<T>().ToMutable(): not on a card yet.
    static AfflictionModel Pending(Sim* sim, AfflictionId id) {
        return AfflictionModel(sim, alloc_affliction(*sim->g, id), true);
    }

    bool HasInstance() const { return (index_ & kHandle) != 0 && sim_ && sim_->g->afflictions.live(index_ & ~kHandle); }
    AfflictionState& st() const { return sim_->g->afflictions[index_ & ~kHandle]; }
    bool IsPending() const { return HasInstance() && st().pending; }
    AfflictionId AfflictionId_() const {
        return HasInstance() ? st().id : AfflictionId::NONE;
    }
    CardModel Card() const;
    bool HasCard() const { return HasInstance() && st().card != 0; }
    int32_t Amount() const { return HasInstance() ? st().amount : 0; }
    uint64_t Uid() const { return HasInstance() ? st().uid : 0; }
    void ReleaseIfPending() const {
        if (IsPending() && sim_->g->afflictions.live(index_ & ~kHandle))
            sim_->g->afflictions.free(index_ & ~kHandle);
    }
    // The Amount setter: recalculates the owner's card values when it changes.
    void SetAmount(int32_t value) const;
    sts2::CombatState CombatState() const;
    // ClearInternal: BeforeRemoved (the card then drops the affliction).
    void ClearInternal() const { BeforeRemoved(); if (HasInstance()) st().card = 0; }
    // PickRandomTargets(rngSet, cards, count)
    CardList PickRandomTargets(const CardList& cards, int32_t count) const;

    // --- overridable members (AfflictionModel.def)
#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const;                                          \
    Ret Name##_Default Params const;
#include "game/Models/AfflictionModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM
};

}  // namespace sts2
