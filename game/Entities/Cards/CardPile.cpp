// Entities/Cards/CardPile.cs (+ PileTypeExtensions.IsCombatPile)
#include "game/Entities/Cards/CardPile.h"

#include "game/All.h"
#include "game/Hooks/Hook.h"

namespace sts2 {

namespace {
// PlayerCombatState.AllPiles order: Hand, Draw, Discard, Exhaust, Play.
int combat_pile_slot(PileType t) {
    switch (t) {
    case PileType::Hand: return 0;
    case PileType::Draw: return 1;
    case PileType::Discard: return 2;
    case PileType::Exhaust: return 3;
    case PileType::Play: return 4;
    default: return -1;
    }
}
}  // namespace

bool IsCombatPile(PileType t) { return static_cast<uint32_t>(static_cast<int>(t) - 1) <= 4u; }

StaticVec<CardRef, kPileCapacity>& CardPile::refs() const {
    if (type_ == PileType::Deck) return sim_->g->run.player.deck;
    int slot = combat_pile_slot(type_);
    if (slot < 0) throw GameError("CardPile has no card list");
    return sim_->g->player_combat.piles[slot];
}

CardList CardPile::Cards() const {
    CardList out;
    for (CardRef r : refs()) out.push_back(CardModel(sim_, r));
    return out;
}

bool CardPile::IsCombatPile() const { return sts2::IsCombatPile(type_); }

int32_t CardPile::UpgradableCardCount() const {
    int32_t n = 0;
    for (CardModel c : Cards()) {
        if (c.IsUpgradable()) ++n;
    }
    return n;
}

CardPile CardPile::Get(PileType type, Player player) {
    switch (type) {
    case PileType::None: return {};
    case PileType::Deck: return CardPile(&player.sim(), player.Index(), PileType::Deck);
    case PileType::Draw:
    case PileType::Hand:
    case PileType::Discard:
    case PileType::Exhaust:
    case PileType::Play:
        if (!player.PlayerCombatState()) return {};  // player.PlayerCombatState?.X
        return CardPile(&player.sim(), player.Index(), type);
    }
    throw GameError("CardPile.Get: unknown pile type");
}

CardList CardPile::GetCards(Player player, std::initializer_list<PileType> piles) {
    CardList out;
    for (PileType p : piles) {
        for (CardModel c : Get(p, player).Cards()) out.push_back(c);
    }
    return out;
}

void CardPile::RandomizeOrderInternal(Player player, Rng& rng, CombatState state) {
    rng.Shuffle(refs());  // _cards.UnstableShuffle(rng)
    CardList cards = Cards();
    Hook::ModifyShuffleOrder(state, player, cards, /*isInitialShuffle=*/true);
    refs().clear();
    for (CardModel c : cards) refs().push_back(c.Index());
}

void CardPile::AddInternal(CardModel card, int32_t index, bool silent) {
    if (refs().contains(card.Index())) throw GameError("Card pile already contains the card.");
    if (index >= 0) {
        refs().insert(static_cast<size_t>(index), card.Index());
    } else {
        refs().push_back(card.Index());
    }
    card.st().pile = type_;  // CardModel.Pile is found by searching the owner's piles
}

void CardPile::RemoveInternal(CardModel card, bool silent) {
    if (!refs().contains(card.Index())) throw GameError("Card pile does not contain the card.");
    refs().remove(card.Index());
    card.st().pile = PileType::None;
}

void CardPile::MoveToBottomInternal(CardModel card) {
    if (!refs().contains(card.Index())) throw GameError("Card pile does not contain the card.");
    refs().remove(card.Index());
    refs().push_back(card.Index());
}

void CardPile::MoveToTopInternal(CardModel card) {
    if (!refs().contains(card.Index())) throw GameError("Card pile does not contain the card.");
    refs().remove(card.Index());
    refs().insert(0, card.Index());
}

void CardPile::Clear(bool silent) {
    for (CardModel item : Cards()) RemoveInternal(item, silent);
    refs().clear();
}

}  // namespace sts2
