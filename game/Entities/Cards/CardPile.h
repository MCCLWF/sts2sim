// Entities/Cards/CardPile.cs. A pile is identified by its owner and type; the
// card list lives in the player's data (Deck) or combat data (the others).
#pragma once

#include <cstdint>

#include "core/rng.h"
#include "engine/sim/sim.h"
#include "game/fwd.h"
#include "game/Models/signature_types.h"

namespace sts2 {

bool IsCombatPile(PileType t);  // PileTypeExtensions.IsCombatPile

class CardPile {
public:
    static constexpr int32_t MaxCardsInHand = 10;

    CardPile() = default;
    CardPile(Sim* sim, uint8_t player, PileType type) : sim_(sim), player_(player), type_(type) {}

    explicit operator bool() const { return player_ != 0; }
    friend bool operator==(const CardPile& a, const CardPile& b) { return a.player_ == b.player_ && a.type_ == b.type_; }
    Sim& sim() const { return *sim_; }
    PileType Type() const { return type_; }
    uint8_t PlayerIndex() const { return player_; }

    // The pile's card list in state (refs, C# order: index 0 is the top of the draw pile).
    StaticVec<CardRef, kPileCapacity>& refs() const;
    CardList Cards() const;  // snapshot
    int32_t Count() const { return static_cast<int32_t>(refs().size()); }
    bool IsEmpty() const { return refs().empty(); }
    bool IsCombatPile() const;
    int32_t UpgradableCardCount() const;

    static CardPile Get(PileType type, sts2::Player player);  // null when the pile does not exist
    static CardList GetCards(sts2::Player player, std::initializer_list<PileType> piles);

    void RandomizeOrderInternal(sts2::Player player, Rng& rng, sts2::CombatState state);
    void AddInternal(CardModel card, int32_t index = -1, bool silent = false);
    void RemoveInternal(CardModel card, bool silent = false);
    void MoveToBottomInternal(CardModel card);
    void MoveToTopInternal(CardModel card);
    void Clear(bool silent = false);

private:
    Sim* sim_ = nullptr;
    uint8_t player_ = 0;
    PileType type_ = PileType::None;
};

}  // namespace sts2
