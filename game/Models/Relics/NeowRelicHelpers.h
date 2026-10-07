#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Runs/CardCreationOptions.h"

namespace sts2::NeowRelicHelpers {

inline CardModel AddCard(Player player, CardId id) {
    CardModel card = player.RunState().CreateCard(id, player);
    return CardPileCmd::Add(card, PileType::Deck).cardAdded;
}

inline CardList CreateCards(Player player, CardPoolId pool, int32_t count, CardRarityOddsType odds = CardRarityOddsType::RegularEncounter) {
    CardCreationOptionsState options = CardCreationOptions::Make({pool}, CardCreationSource::Other, odds);
    CardCreationOptions::WithFlags(options, CardCreationFlags::NoUpgradeRoll);
    return CardFactory::CreateForReward(player, count, options);
}

inline CardModel ChooseAndAdd(Player player, CardList cards) {
    PlayerChoiceContext context;
    CardModel chosen = CardSelectCmd::FromChooseACardScreen(context, cards, player, true);
    if (chosen) CardPileCmd::Add(chosen, PileType::Deck);
    return chosen;
}

inline void OfferRelics(Player player, int32_t count) {
    StaticVec<Reward, 8> rewards;
    for (int32_t i = 0; i < count; ++i) rewards.push_back(Reward::NewRelicReward(player));
    RewardsCmd::OfferCustom(player, rewards);
}

}  // namespace sts2::NeowRelicHelpers
