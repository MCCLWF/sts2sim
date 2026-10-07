// Value types that appear in overridable member signatures (the *.def
// tables). C# collection parameters become fixed-capacity lists of handles.
#pragma once

#include "content/generated/enums.h"
#include "engine/containers.h"
#include "engine/map.h"
#include "engine/sim/state.h"
#include "game/fwd.h"

namespace sts2 {

// MerchantEntry's stable handle in the simulator's fixed-size shop state.
namespace Merchant {
struct EntryRef {
    enum Kind : uint8_t { CharacterCard, ColorlessCard, Relic, Potion, CardRemoval } kind = CharacterCard;
    uint8_t index = 0;
    friend bool operator==(EntryRef, EntryRef) = default;
};
}  // namespace Merchant

// A card's keywords (HashSet<CardKeyword> / ISet<CardKeyword>).
struct CardKeywordSet {
    uint64_t bits = 0;
    bool Contains(CardKeyword k) const { return (bits >> static_cast<unsigned>(k)) & 1u; }
    void Add(CardKeyword k) { bits |= uint64_t{1} << static_cast<unsigned>(k); }
    void Remove(CardKeyword k) { bits &= ~(uint64_t{1} << static_cast<unsigned>(k)); }
    friend bool operator==(const CardKeywordSet&, const CardKeywordSet&) = default;
};

// IReadOnlySet<RoomType>, represented as a fixed bit set. RoomType values fit
// in 16 bits; callers that need enum order can scan the bits from low to high.
struct RoomTypeSet {
    uint16_t bits = 0;
    bool Contains(RoomType room) const { return (bits >> static_cast<unsigned>(room)) & 1u; }
    void Add(RoomType room) { bits |= uint16_t{1} << static_cast<unsigned>(room); }
    void Remove(RoomType room) { bits &= ~(uint16_t{1} << static_cast<unsigned>(room)); }
    friend bool operator==(const RoomTypeSet&, const RoomTypeSet&) = default;
};

inline constexpr int kMaxCreatureList = 16;
// AllCards spans every combat pile, so one pile's capacity is insufficient.
// Temporary handle lists must represent every live card in the modeled pool.
inline constexpr int kMaxCardList = kCardPool - 1;

// IEnumerable<Creature> / IReadOnlyList<Creature> and card collections in
// signatures. The element handles are complete wherever the lists are used.
using CreatureList = StaticVec<Creature, kMaxCreatureList>;
using CardList = StaticVec<CardModel, kMaxCardList>;

// Sets an encounter's fields on a freshly created monster (see EncounterModel).

}  // namespace sts2
