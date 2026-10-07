// Runs/CardCreationOptions.cs. The record is the POD CardCreationOptionsState
// (engine/sim/state.h); these are its members.
#pragma once

#include "game/All.h"

namespace sts2::CardCreationOptions {

using Options = CardCreationOptionsState;
// Serializable source predicates for the normal card-pool filter. The value is
// stored in CardCreationOptionsState so reward generation can resume after a
// snapshot restore without retaining a C++ callback.
inline constexpr uint8_t ZeroCanonicalCostNonX = 1;
inline constexpr uint8_t OnlyMultiplayerCards = 2;

Options Make(std::initializer_list<CardPoolId> cardPools, CardCreationSource source, CardRarityOddsType rarityOdds);
Options ForRoom(Player player, RoomType roomType);
Options ForNonCombatWithDefaultOdds(std::initializer_list<CardPoolId> cardPools);
Options ForNonCombatWithUniformOdds(std::initializer_list<CardPoolId> cardPools);

inline Options& WithFlags(Options& o, CardCreationFlags flag) {
    o.flags = o.flags | flag;
    return o;
}
inline Options& WithRarityOdds(Options& o, CardRarityOddsType odds) {
    o.rarity_odds = odds;
    return o;
}

// GetPossibleCards(player): the pools' unlocked cards (every card on the
// fully unlocked profile) passing the multiplayer constraint and the filter.
StaticVec<CardId, 640> GetPossibleCards(const Options& o, Player player);

}  // namespace sts2::CardCreationOptions
