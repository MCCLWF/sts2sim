// Canonical models (ModelDb.Card<T>(), a relic in a grab bag, ...). The C#
// canonical instance is an immutable template, distinct from ToMutable().
// Scratch storage supplies fields while the wrapper is alive. Its handle
// preserves the template's definition identity; scratch allocation/release
// must never be reported as a native model object's creation/destruction.
#pragma once

#include "engine/sim/alloc.h"
#include "game/All.h"

namespace sts2 {

class CanonicalCard {
public:
    CanonicalCard(Sim* sim, CardId id)
        : sim_(sim), capacity_(sim->g->cards.capacity()), ref_(alloc_card(*sim->g, id, 0)) {}
    ~CanonicalCard() {
        sim_->g->cards.free(ref_);
        sim_->g->cards.trim_capacity(capacity_);
    }
    CanonicalCard(const CanonicalCard&) = delete;
    CanonicalCard& operator=(const CanonicalCard&) = delete;
    CardModel operator*() const { return get(); }
    CardModel get() const { return AbstractModel::CanonicalView(CardModel(sim_, ref_),static_cast<uint16_t>(sim_->g->cards[ref_].id)); }

private:
    Sim* sim_;
    uint32_t capacity_;
    CardRef ref_;
};

class CanonicalRelic {
public:
    CanonicalRelic(Sim* sim, RelicId id)
        : sim_(sim), capacity_(sim->g->relics.capacity()), ref_(alloc_relic(*sim->g, id, 0)) {}
    ~CanonicalRelic() {
        sim_->g->relics.free(ref_);
        sim_->g->relics.trim_capacity(capacity_);
    }
    CanonicalRelic(const CanonicalRelic&) = delete;
    CanonicalRelic& operator=(const CanonicalRelic&) = delete;
    RelicModel get() const { return AbstractModel::CanonicalView(RelicModel(sim_, ref_),static_cast<uint16_t>(sim_->g->relics[ref_].id)); }

private:
    Sim* sim_;
    uint32_t capacity_;
    RelicRef ref_;
};

}  // namespace sts2
