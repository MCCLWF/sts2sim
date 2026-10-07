// Runs/RelicGrabBag.cs
#include "game/Runs/RelicGrabBag.h"

#include "game/Models/Canonical.h"

namespace sts2::RelicGrabBagOps {

namespace {

using Deque = StaticVec<RelicId, 160>;

bool Passes(const RelicFilter& filter, RelicId r) { return !filter || filter(r); }

bool IsAllowed(Sim* sim, RelicId relic) {
    CanonicalRelic canonical(sim, relic);
    return canonical.get().IsAllowed(RunState(sim));
}

void RemoveDisallowedRelicsFromDeques(Sim* sim, RelicGrabBag& bag) {
    for (auto& d : bag.deques) {
        for (size_t i = 0; i < d.relics.size(); ++i) {
            if (!IsAllowed(sim, d.relics[i])) {
                d.relics.erase_at(i);
                --i;
            }
        }
    }
    for (size_t j = 0; j < bag.mp_fallback.size(); ++j) {
        if (!IsAllowed(sim, bag.mp_fallback[j])) {
            bag.mp_fallback.erase_at(j);
            --j;
        }
    }
}

void RefreshRarity(RelicGrabBag& bag, RelicRarity rarity) {
    if (bag.original.empty()) throw GameError("Tried to refresh relics but original list is null");
    for (RelicId r : bag.original) {
        if (data(r).rarity != rarity) continue;
        RelicGrabBag::Deque* d = bag.find(rarity);
        if (d == nullptr) {
            bag.deques.push_back(RelicGrabBag::Deque{.rarity = rarity});
            d = &bag.deques.back();
        }
        d->relics.push_back(r);
    }
}

template <class List>
bool DequeHasAnyRelics(const List& deque, const RelicFilter& filter) {
    for (RelicId r : deque) {
        if (Passes(filter, r)) return true;
    }
    return false;
}

// The list GetAvailableDeque returns: a deque of the bag, the multiplayer
// fallback, or none. GetDeque of a missing rarity returns a fresh empty list
// that is not stored in the bag (`detached`).
struct DequeRef {
    enum Kind : uint8_t { None, Rarity, Fallback, Detached } kind = None;
    RelicRarity rarity = RelicRarity::None;
};

DequeRef GetDeque(RelicGrabBag& bag, RelicRarity rarity) {
    if (bag.find(rarity) == nullptr) return {DequeRef::Detached, rarity};
    return {DequeRef::Rarity, rarity};
}

bool DequeHasAnyRelics(RelicGrabBag& bag, DequeRef d, const RelicFilter& filter) {
    switch (d.kind) {
    case DequeRef::Rarity: return DequeHasAnyRelics(bag.find(d.rarity)->relics, filter);
    case DequeRef::Fallback: return DequeHasAnyRelics(bag.mp_fallback, filter);
    default: return false;
    }
}

size_t Count(RelicGrabBag& bag, DequeRef d) {
    switch (d.kind) {
    case DequeRef::Rarity: return bag.find(d.rarity)->relics.size();
    case DequeRef::Fallback: return bag.mp_fallback.size();
    default: return 0;
    }
}

DequeRef GetAvailableDeque(Sim* sim, RelicGrabBag& bag, RelicRarity rarity, const RelicFilter& filter) {
    RemoveDisallowedRelicsFromDeques(sim, bag);
    DequeRef list = GetDeque(bag, rarity);
    if (Count(bag, list) == 0 && bag.refresh_allowed) {
        RefreshRarity(bag, rarity);
        RemoveDisallowedRelicsFromDeques(sim, bag);
    }
    while (list.kind != DequeRef::None && !DequeHasAnyRelics(bag, list, filter)) {
        switch (rarity) {
        case RelicRarity::Shop: rarity = RelicRarity::Common; break;
        case RelicRarity::Common: rarity = RelicRarity::Uncommon; break;
        case RelicRarity::Uncommon: rarity = RelicRarity::Rare; break;
        default: rarity = RelicRarity::None; break;
        }
        list = rarity == RelicRarity::None ? DequeRef{} : GetDeque(bag, rarity);
    }
    if (list.kind == DequeRef::None && DequeHasAnyRelics(bag.mp_fallback, filter)) list = {DequeRef::Fallback, RelicRarity::None};
    return list;
}

template <class List>
RelicId TakeFront(List& deque, const RelicFilter& filter) {
    for (size_t i = 0; i < deque.size(); ++i) {
        RelicId r = deque[i];
        if (Passes(filter, r)) {
            deque.erase_at(i);
            return r;
        }
    }
    return RelicId::NONE;
}

template <class List>
RelicId TakeBack(List& deque, const RelicFilter& filter) {
    for (size_t i = deque.size(); i-- > 0;) {
        RelicId r = deque[i];
        if (Passes(filter, r)) {
            deque.erase_at(i);
            return r;
        }
    }
    return RelicId::NONE;
}

}  // namespace

RelicId PullFromFront(Sim* sim, RelicGrabBag& bag, RelicRarity rarity, const RelicFilter& filter) {
    DequeRef d = GetAvailableDeque(sim, bag, rarity, filter);
    if (Count(bag, d) == 0) return RelicId::NONE;
    return d.kind == DequeRef::Fallback ? TakeFront(bag.mp_fallback, filter) : TakeFront(bag.find(d.rarity)->relics, filter);
}

RelicId PullFromBack(Sim* sim, RelicGrabBag& bag, RelicRarity rarity, const RelicFilter& filter) {
    DequeRef d = GetAvailableDeque(sim, bag, rarity, filter);
    if (Count(bag, d) == 0) return RelicId::NONE;
    return d.kind == DequeRef::Fallback ? TakeBack(bag.mp_fallback, filter) : TakeBack(bag.find(d.rarity)->relics, filter);
}

void Remove(RelicGrabBag& bag, RelicId relic) {
    for (auto& d : bag.deques) {
        for (size_t i = 0; i < d.relics.size();) {
            if (d.relics[i] == relic) {
                d.relics.erase_at(i);
            } else {
                ++i;
            }
        }
    }
}

bool Contains(const RelicGrabBag& bag, RelicId relic) {
    for (const auto& d : bag.deques) {
        if (d.relics.contains(relic)) return true;
    }
    return false;
}

// HasAvailableRelics: any rarity of _rarities with an available deque, or an
// allowed relic in the multiplayer fallback.
bool HasAvailableRelics(Sim* sim, RelicGrabBag& bag) {
    static constexpr RelicRarity kRarities[] = {RelicRarity::Common, RelicRarity::Uncommon, RelicRarity::Rare, RelicRarity::Shop};
    for (RelicRarity r : kRarities) {
        if (GetAvailableDeque(sim, bag, r, nullptr).kind != DequeRef::None) return true;
    }
    for (RelicId r : bag.mp_fallback) {
        if (IsAllowed(sim, r)) return true;
    }
    return false;
}

}  // namespace sts2::RelicGrabBagOps
