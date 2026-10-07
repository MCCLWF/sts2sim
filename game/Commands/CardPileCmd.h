// Commands/CardPileCmd.cs. Card nodes, tweens and waits are presentation and
// are dropped; with TestMode on (the oracle) the tweens are null, so the
// post-tween hooks always run.
#pragma once

#include "game/All.h"

namespace sts2 {

// Entities/Cards/CardPileAddResult.cs. modifyingModels is left out: only the
// card-reward UI reads it.
struct CardPileAddResult {
    bool success = false;
    CardModel cardAdded;
    CardPile oldPile;  // null if the card was in no pile
    PileType targetPile = PileType::None;
};
using CardPileAddResults = StaticVec<CardPileAddResult, kMaxCardList>;

}  // namespace sts2

namespace sts2::CardPileCmd {

// AddCurseToDeck<T>(owner) / AddCursesToDeck(curses, owner): curses are canonical.
CardModel AddCurseToDeck(CardId curse, Player owner);
CardPileAddResults AddCursesToDeck(const StaticVec<CardId, 8>& curses, Player owner);
// RemoveFromDeck(card(s), showPreview): the preview is the UI's.
void RemoveFromDeck(CardModel card, bool showPreview = true);
void RemoveFromDeck(const CardList& cards, bool showPreview = true);
void RemoveFromCombat(CardModel card, bool skipVisuals = false);
void RemoveFromCombat(const CardList& cards, bool skipVisuals = false);

CardPileAddResult AddGeneratedCardToCombat(CardModel card, PileType newPileType, Player creator,
                                           CardPilePosition position = CardPilePosition::Bottom);
CardPileAddResults AddGeneratedCardsToCombat(const CardList& cards, PileType newPileType, Player creator,
                                             CardPilePosition position = CardPilePosition::Bottom);

CardPileAddResult Add(CardModel card, PileType newPileType, CardPilePosition position = CardPilePosition::Bottom,
                      AbstractModel clonedBy = {}, bool skipVisuals = false);
CardPileAddResult Add(CardModel card, CardPile newPile, CardPilePosition position = CardPilePosition::Bottom,
                      AbstractModel clonedBy = {}, bool skipVisuals = false);
CardPileAddResults Add(const CardList& cards, PileType newPileType, CardPilePosition position = CardPilePosition::Bottom,
                       AbstractModel clonedBy = {}, bool skipVisuals = false);
CardPileAddResults Add(const CardList& cards, CardPile newPile, CardPilePosition position = CardPilePosition::Bottom,
                       AbstractModel clonedBy = {}, bool skipVisuals = false, bool isChangingOwners = false);

void AddDuringManualCardPlay(CardModel card);

CardModel Draw(PlayerChoiceContext& choiceContext, Player player);
CardList Draw(PlayerChoiceContext& choiceContext, Dec count, Player player, bool fromHandDraw = false);

void Shuffle(PlayerChoiceContext& choiceContext, Player player);
// ListExtensions.StableShuffle: sort by CardModel.CompareTo, then shuffle.
void StableShuffle(CardList& cards, Rng& rng);

// AddToCombatAndPreview<T>: `card` is T. The preview is presentation.
void AddToCombatAndPreview(CardId card, const CreatureList& targets, PileType pileType, int32_t count, Player creator,
                           CardPilePosition position = CardPilePosition::Bottom);
void AddToCombatAndPreview(CardId card, Creature target, PileType pileType, int32_t count, Player creator,
                           CardPilePosition position = CardPilePosition::Bottom);
void ShuffleIfNecessary(PlayerChoiceContext& choiceContext, Player player);

// CardPileCmd.AutoPlayFromDrawPile: play the top cards directly from draw.
// The card is marked ExhaustOnNextPlay when forceExhaust is true.
void AutoPlayFromDrawPile(PlayerChoiceContext& choiceContext, Player player, int32_t count,
                          CardPilePosition position = CardPilePosition::Top, bool forceExhaust = false);

}  // namespace sts2::CardPileCmd
