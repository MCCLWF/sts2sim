// Creating model instances in the state pools (the C# ToMutable() of a
// canonical model). Combat-specific setup (piles, CombatState registration)
// is the caller's job.
#pragma once

#include "engine/sim/state.h"

namespace sts2 {

// CardModel.ToMutable(): a fresh card at upgrade level 0.
inline CardRef alloc_card(GameState& g, CardId id, uint8_t owner) {
    const CardData& d = data(id);
    CardRef r = g.cards.alloc();
    CardState& c = g.cards[r];
    c.id = id;
    c.owner = owner;
    c.cost_base = static_cast<int32_t>(d.costs_x ? 0 : d.levels[0].energy_cost);
    c.keywords = d.levels[0].keywords;
    c.base_star_cost = static_cast<int32_t>(d.star_cost);
    return r;
}

// ModelDb.Affliction<T>().ToMutable(): a distinct writable object, even
// before it is attached to a card. The monotonically increasing uid survives
// pool-slot reuse and state cloning; zero is reserved for null.
inline AfflictionRef alloc_affliction(GameState& g, AfflictionId id) {
    if (g.next_affliction_uid == 0 || g.next_affliction_uid == UINT64_MAX) throw CapacityError("affliction uid exhausted");
    AfflictionRef r = g.afflictions.alloc();
    AfflictionState& a = g.afflictions[r];
    a.id = id;
    a.pending = true;
    a.uid = g.next_affliction_uid++;
    return r;
}

// CardModel.ClonePreservingMutability(): attached AfflictionModel instances
// are deep-cloned, while the source object remains valid for its original
// card. The clone receives a fresh UID and card root.
inline AfflictionRef clone_affliction(GameState& g, AfflictionRef source, CardRef card) {
    if (!source) return 0;
    if (!g.afflictions.live(source)) throw GameError("cloning a dead AfflictionModel");
    if (g.next_affliction_uid == 0 || g.next_affliction_uid == UINT64_MAX) throw CapacityError("affliction uid exhausted");
    AfflictionRef copy = g.afflictions.alloc();
    g.afflictions[copy] = g.afflictions[source];
    g.afflictions[copy].uid = g.next_affliction_uid++;
    g.afflictions[copy].card = card;
    g.afflictions[copy].pending = false;
    return copy;
}

inline RelicRef alloc_relic(GameState& g, RelicId id, uint8_t owner) {
    const RelicData& d = data(id);
    RelicRef r = g.relics.alloc();
    RelicState& s = g.relics[r];
    s.id = id;
    s.owner = owner;
    for (size_t i = 0; i < d.vars.size() && i < kMaxModelVars; ++i) s.vars[i] = d.vars[i].base;
    return r;
}

// PowerModel.ToMutable(): amount 0, vars from the table.
inline PowerRef alloc_power(GameState& g, PowerId id) {
    const PowerData& d = data(id);
    PowerRef r = g.powers.alloc();
    PowerState& s = g.powers[r];
    s.id = id;
    for (size_t i = 0; i < d.vars.size() && i < kMaxModelVars; ++i) s.vars[i] = d.vars[i].base;
    if (d.vars.size() > kMaxModelVars) throw CapacityError("power has more vars than kMaxModelVars");
    if (id == PowerId::AUTOMATION_POWER) s.extra[0] = 10;  // AutomationPower.Data.cardsLeft initializer
    return r;
}

inline PotionRef alloc_potion(GameState& g, PotionId id, uint8_t owner) {
    PotionRef r = g.potions.alloc();
    g.potions[r].id = id;
    g.potions[r].owner = owner;
    return r;
}

}  // namespace sts2
