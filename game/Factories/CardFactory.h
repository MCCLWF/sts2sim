// Factories/CardFactory.cs (the parts ported so far).
#pragma once

#include <cstdint>
#include <vector>

#include "game/All.h"

namespace sts2::CardFactory {

// CreateForReward(player, cardCount, options): the cards, created in the run
// state (RunState.CreateCard), with their upgrade rolls. CardCreationResult is
// its Card (the modifying relics are only shown in the UI).
// C# CardCreationOptions is mutable. The mutable overload retains changes
// made by creation hooks for the caller and later reward hooks.
CardList CreateForReward(Player player, int32_t cardCount, CardCreationOptionsState& options);
CardList CreateForReward(Player player, int32_t cardCount, const CardCreationOptionsState& options);
// Crystal Sphere passes its event RNG through CardCreationOptions.WithRngOverride.
CardList CreateForReward(Player player, int32_t cardCount, CardCreationOptionsState& options, Rng& rng);
CardList CreateForReward(Player player, int32_t cardCount, const CardCreationOptionsState& options, Rng& rng);
// CardPoolModel.GetUnlockedCards(unlockState, SingleplayerOnly): the fully
// unlocked profile, without the MultiplayerOnly cards.
StaticVec<CardId, 640> GetUnlockedCards(CardPoolId pool);
StaticVec<CardId, 640> GetUnlockedCards(CardPoolId pool, uint64_t unlocked_epoch_bits);
StaticVec<CardId, 640> GetUnlockedCards(CardPoolId pool, Player player);
bool IsUnlockedByEpoch(CardId card, uint64_t unlocked_epoch_bits);

// CreateForMerchant(player, options, type) and (player, options, rarity):
// `options` are canonical cards; the card is created in the run state.
CardModel CreateForMerchant(Player player, const StaticVec<CardId, 640>& options, CardType type);
CardModel CreateForMerchant(Player player, const StaticVec<CardId, 640>& options, CardRarity rarity);

// GetDefaultTransformationOptions / CreateRandomCardForTransform (the card is
// created in the card's scope: the combat while its owner is in one, else the run).
std::vector<CardId> GetDefaultTransformationOptions(CardModel original, bool isInCombat);
CardModel CreateRandomCardForTransform(CardModel original, bool isInCombat, Rng& rng);
CardModel CreateRandomCardForTransform(CardModel original, const std::vector<CardId>& options, bool isInCombat, Rng& rng);

// Combat generation helpers used by Ironclad cards (Stoke, Infernal Blade).
// They mirror CardFactory.GetForCombat/GetDistinctForCombat over a card pool.
CardList GetForCombat(Player player, CardPoolId pool, int32_t count, Rng& rng, CardType type = CardType::None,
                       bool distinct = false, CardId excluded = CardId::NONE, CardRarity rarity = CardRarity::None);
// Splash supplies the other unlocked character pools as one ordered candidate
// set. Filter each pool's unlocked cards, concatenate in the caller's pool
// order, then apply the same combat filter and one TakeRandom shuffle.
CardList GetForCombat(Player player, const StaticVec<CardPoolId, 8>& pools, int32_t count, Rng& rng,
                      CardType type = CardType::None, bool distinct = true, CardId excluded = CardId::NONE,
                      CardRarity rarity = CardRarity::None);
// Jackpot: first filters unlocked cards to zero canonical cost and non-X,
// then applies the regular combat-generation filters and samples with replacement.
CardList GetForCombatZeroCost(Player player, CardPoolId pool, int32_t count, Rng& rng);

}  // namespace sts2::CardFactory
