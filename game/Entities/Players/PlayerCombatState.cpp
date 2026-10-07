// Entities/Players/PlayerCombatState.cs
#include "game/Entities/Players/PlayerCombatState.h"

#include <algorithm>

#include "game/All.h"
#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Hooks/Hook.h"

namespace sts2 {

sts2::Player PlayerCombatState::Player() const { return sts2::Player(sim_, player_); }

StaticVec<Creature, 8> PlayerCombatState::Pets() const {
    StaticVec<Creature, 8> out;
    for (CreatureRef c : st().pets) out.push_back(Creature(sim_, c));
    return out;
}

CardPile PlayerCombatState::Hand() const { return CardPile(sim_, player_, PileType::Hand); }
CardPile PlayerCombatState::DrawPile() const { return CardPile(sim_, player_, PileType::Draw); }
CardPile PlayerCombatState::DiscardPile() const { return CardPile(sim_, player_, PileType::Discard); }
CardPile PlayerCombatState::ExhaustPile() const { return CardPile(sim_, player_, PileType::Exhaust); }
CardPile PlayerCombatState::PlayPile() const { return CardPile(sim_, player_, PileType::Play); }

StaticVec<CardPile, 5> PlayerCombatState::AllPiles() const {
    return StaticVec<CardPile, 5>{Hand(), DrawPile(), DiscardPile(), ExhaustPile(), PlayPile()};
}

CardList PlayerCombatState::AllCards() const {
    CardList out;
    for (CardPile p : AllPiles()) {
        for (CardModel c : p.Cards()) out.push_back(c);
    }
    return out;
}

int32_t PlayerCombatState::MaxEnergy() const {
    sts2::Player p = Player();
    return Hook::ModifyMaxEnergy(p.Creature().CombatState(), p, Dec(p.MaxEnergy())).to_int();
}

// PlayerCombatState.Stars: record the actual signed delta after mutating the
// resource, matching StarsModifiedEntry creation order in the source game.
void PlayerCombatState::SetStars(int32_t v) {
    const int32_t previous = st().stars;
    if (previous == v) return;
    st().stars = v;
    CombatState combatState = Player().Creature().CombatState();
    // Room-entry relics can modify stars after SetUpCombat but before the
    // first turn begins. The original combat history records those changes,
    // and formulas such as Radiate still treat them as this turn's history.
    CombatManager combatManager(sim_);
    if (combatState && (combatManager.IsStarting() || combatManager.IsInProgress()))
        combatManager.History().StarsModified(combatState, v - previous, Player());
}

void PlayerCombatState::AfterCombatEnd() {
    for (CardPile p : AllPiles()) p.Clear();
    st().pets.clear();
}

void PlayerCombatState::ResetEnergy() { SetEnergy(MaxEnergy()); }

void PlayerCombatState::AddMaxEnergyToCurrent() { SetEnergy(Energy() + MaxEnergy()); }

void PlayerCombatState::LoseEnergy(Dec amount) {
    if (amount < Dec(0)) throw GameError("Must not be negative.");
    SetEnergy(dec_min(dec_max(Dec(Energy()) - amount, Dec(0)), Dec(999999999)).to_int());
}

void PlayerCombatState::GainEnergy(Dec amount) {
    if (amount < Dec(0)) throw GameError("Must not be negative.");
    SetEnergy(dec_min(dec_max(Dec(Energy()) + amount, Dec(0)), Dec(999999999)).to_int());
}

bool PlayerCombatState::HasEnoughResourcesFor(CardModel card, UnplayableReason& reason) const {
    int32_t num = std::max(0, card.EnergyCost().GetWithModifiers(CostModifiers::All));
    int32_t num2 = std::max(0, card.GetStarCostWithModifiers());
    if (num > Energy() && card.CombatState() && Hook::ShouldPayExcessEnergyCostWithStars(card.CombatState(), Player())) {
        num2 += (num - Energy()) * 2;
        num = Energy();
    }
    reason = UnplayableReason::None;
    if (num > Energy()) reason = reason | UnplayableReason::EnergyCostTooHigh;
    if (num2 > Stars()) reason = reason | UnplayableReason::StarCostTooHigh;
    return reason == UnplayableReason::None;
}

void PlayerCombatState::LoseStars(Dec amount) {
    if (amount < Dec(0)) throw GameError("Must not be negative.");
    SetStars(dec_max(Dec(Stars()) - amount, Dec(0)).to_int());
}

void PlayerCombatState::GainStars(Dec amount) {
    if (amount < Dec(0)) throw GameError("Must not be negative.");
    SetStars(dec_max(Dec(Stars()) + amount, Dec(0)).to_int());
}

void PlayerCombatState::AddPetInternal(Creature pet) {
    if (st().pets.contains(pet.Index())) return;
    if (pet.PetOwner() != Player()) pet.st().pet_owner = player_;
    st().pets.push_back(pet.Index());  // pet.Died += OnPetDied: see Creature::InvokeDiedEvent
}

Creature PlayerCombatState::GetPet(MonsterId id) const {
    for (Creature p : Pets()) {
        if (p.Monster().MonsterId_() == id) return p;
    }
    return {};
}

void PlayerCombatState::RecalculateCardValues() {
    for (CardModel c : AllCards()) {
        if (EnchantmentModel e = c.Enchantment()) e.RecalculateValues();
    }
}

void PlayerCombatState::EndOfTurnCleanup() {
    for (CardModel c : AllCards()) c.EndOfTurnCleanup();
}

bool PlayerCombatState::HasCardsToPlay() const {
    for (CardModel c : Hand().Cards()) {
        if (c.CanPlay()) return true;
    }
    return false;
}

void PlayerCombatState::OnPetDied(Creature pet) {
    if (!st().pets.contains(pet.Index())) throw GameError("Player does not have the pet");
    if (Hook::ShouldCreatureBeRemovedFromCombatAfterDeath(pet.CombatState(), pet)) st().pets.remove(pet.Index());
}

}  // namespace sts2
