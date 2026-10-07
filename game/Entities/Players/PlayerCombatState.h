// Entities/Players/PlayerCombatState.cs
#pragma once

#include <cstdint>

#include "core/dec.h"
#include "engine/sim/sim.h"
#include "game/fwd.h"
#include "game/Models/signature_types.h"

namespace sts2 {

class PlayerCombatState {
public:
    PlayerCombatState() = default;
    PlayerCombatState(Sim* sim, uint8_t player) : sim_(sim), player_(player) {}

    explicit operator bool() const { return player_ != 0 && sim_->g->player_combat.exists; }
    Sim& sim() const { return *sim_; }
    PlayerCombatData& st() const { return sim_->g->player_combat; }
    sts2::Player Player() const;

    StaticVec<Creature, 8> Pets() const;
    int32_t TurnNumber() const { return st().turn_number; }
    PlayerTurnPhase Phase() const { return st().phase; }
    void SetPhase(PlayerTurnPhase phase) { st().phase = phase; }
    CardPile Hand() const;
    CardPile DrawPile() const;
    CardPile DiscardPile() const;
    CardPile ExhaustPile() const;
    CardPile PlayPile() const;
    StaticVec<CardPile, 5> AllPiles() const;  // Hand, Draw, Discard, Exhaust, Play
    CardList AllCards() const;
    int32_t Energy() const { return st().energy; }
    void SetEnergy(int32_t v) { st().energy = v; }
    int32_t MaxEnergy() const;  // Hook.ModifyMaxEnergy
    int32_t Stars() const { return st().stars; }
    void SetStars(int32_t v);

    void AfterCombatEnd();
    void IncrementTurnNumber() { ++st().turn_number; }
    void ResetEnergy();
    void AddMaxEnergyToCurrent();
    void LoseEnergy(Dec amount);
    void GainEnergy(Dec amount);
    bool HasEnoughResourcesFor(CardModel card, UnplayableReason& reason) const;
    void LoseStars(Dec amount);
    void GainStars(Dec amount);
    void AddPetInternal(Creature pet);
    Creature GetPet(MonsterId id) const;
    void RecalculateCardValues();
    void EndOfTurnCleanup();
    bool HasCardsToPlay() const;
    void OnPetDied(Creature pet);  // Creature.Died subscriber

private:
    Sim* sim_ = nullptr;
    uint8_t player_ = 0;
};

}  // namespace sts2
