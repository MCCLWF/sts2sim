// Commands/CardCmd.cs. Previews and card-node visuals are dropped.
#pragma once

#include <optional>
#include <vector>

#include "game/All.h"
#include "game/Commands/CardPileCmd.h"

namespace sts2 {

// Entities/Cards/CardTransformation.cs: a card to transform, to a given
// replacement, to a random card of the given options, or to a random card of
// its default options.
struct CardTransformation {
    CardModel Original;
    CardModel Replacement;                  // null unless given
    bool HasReplacementOptions = false;
    std::vector<CardId> ReplacementOptions;  // canonical cards
    bool IsInCombat = false;

    explicit CardTransformation(CardModel original);
    CardTransformation(CardModel original, std::vector<CardId> options);
    CardTransformation(CardModel original, CardModel replacement);
    CardModel GetReplacement(Rng* rng) const;
};

}  // namespace sts2

namespace sts2::CardCmd {

// EnchantmentModel.CanEnchant: the shared source predicate used both when an
// enchantment is applied and when another effect needs to preserve one.
bool CanEnchant(EnchantmentId enchantment, CardModel card);

// Apply one of the card enchantments. The state stores the enchantment on the
// card itself, so no separate model allocation is needed.
EnchantmentModel Enchant(EnchantmentId enchantment, CardModel card, Dec amount);
// Clone the mutable enchantment facts when a source effect creates a different
// card with the same enchantment (ArchaicTooth.GetTranscendenceTransformedCard).
EnchantmentModel CloneEnchantment(CardModel source, CardModel target);

void AutoPlay(PlayerChoiceContext& choiceContext, CardModel card, Creature target, AutoPlayType type = AutoPlayType::Default,
              bool skipXCapture = false, bool skipCardPileVisuals = false);
void Discard(PlayerChoiceContext& choiceContext, CardModel card);
void Discard(PlayerChoiceContext& choiceContext, const CardList& cards);
void DiscardAndDraw(PlayerChoiceContext& choiceContext, const CardList& cardsToDiscard, int32_t cardsToDraw);
// Task<CardPileAddResult?>: `has_value` is false for the null result.
std::optional<CardPileAddResult> Exhaust(PlayerChoiceContext& choiceContext, CardModel card, bool causedByEthereal = false,
                                         bool skipVisuals = false);
void Upgrade(CardModel card);
void Upgrade(const CardList& cards);
void Downgrade(CardModel card);
void ApplyKeyword(CardModel card, std::initializer_list<CardKeyword> keywords);
void RemoveKeyword(CardModel card, std::initializer_list<CardKeyword> keywords);
void ApplySingleTurnSly(CardModel card);
void ApplySingleTurnRetain(CardModel card);

// Afflict<T>(card, amount) / Afflict(affliction, card, amount): the card's
// affliction, or null. AfflictAndPreview<T>(cards, amount): the preview is
// the UI's. ClearAffliction(card).
AfflictionModel Afflict(AfflictionId affliction, CardModel card, Dec amount);
AfflictionModel Afflict(AfflictionModel affliction, CardModel card, Dec amount);
std::vector<AfflictionModel> AfflictAndPreview(AfflictionId affliction, const CardList& cards, Dec amount);
void ClearAffliction(CardModel card);

// TransformToRandom(original, rng) / Transform(original, replacement) /
// Transform(transformations, rng). Previews (CardPreviewStyle) are the UI's.
CardPileAddResult TransformToRandom(CardModel original, Rng& rng);
std::optional<CardPileAddResult> Transform(CardModel original, CardModel replacement);
std::vector<CardPileAddResult> Transform(const std::vector<CardTransformation>& transformations, Rng* rng);

}  // namespace sts2::CardCmd
