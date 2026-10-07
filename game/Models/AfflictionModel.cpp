// Models/AfflictionModel.cs
#include "game/Models/AfflictionModel.h"

#include <algorithm>

#include "game/All.h"

namespace sts2 {

CardModel AfflictionModel::Card() const { return HasCard() ? CardModel(sim_, st().card) : CardModel{}; }

CombatState AfflictionModel::CombatState() const { return Card().CombatState(); }

void AfflictionModel::SetAmount(int32_t value) const {
    if (!HasInstance()) throw GameError("AfflictionModel: invalid instance");
    if (st().amount == value) return;
    st().amount = value;
    if (HasCard()) {
        sim_->g->cards[st().card].affliction_amount = static_cast<int32_t>(value);
        Card().Owner().PlayerCombatState().RecalculateCardValues();
    }
}

bool AfflictionModel::CanAfflict_Default(CardModel card) const {
    if (!CanAfflictCardType(card.Type())) return false;
    if (card.Keywords().Contains(CardKeyword::Unplayable) && !CanAfflictUnplayableCards()) return false;
    AfflictionModel current = card.Affliction();
    if (current && (!IsStackable() || current.AfflictionId_() != AfflictionId_())) return false;
    return true;
}

CardList AfflictionModel::PickRandomTargets(const CardList& cards, int32_t count) const {
    // cards.Where(CanAfflict).ToList().UnstableShuffle(rngSet.CombatCardGeneration)
    CardList list;
    for (CardModel c : cards) {
        if (CanAfflict(c)) list.push_back(c);
    }
    sim_->g->run.rng.CombatCardGeneration().Shuffle(list);
    // list.RemoveRange(Math.Clamp(list.Count - 1, 0, count), Math.Max(0, list.Count - count))
    int32_t n = static_cast<int32_t>(list.size());
    int32_t start = std::clamp(n - 1, 0, count);
    int32_t remove = std::max(0, n - count);
    CardList out;
    for (int32_t i = 0; i < n; ++i) {
        if (i >= start && i < start + remove) continue;
        out.push_back(list[static_cast<size_t>(i)]);
    }
    return out;
}

}  // namespace sts2
