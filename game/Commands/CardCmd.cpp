// Commands/CardCmd.cs
#include "game/Commands/CardCmd.h"

#include <algorithm>
#include <string>

#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Hooks/Hook.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/Enchantments/Corrupted.h"
#include "game/Models/Enchantments/Goopy.h"
#include "game/Models/Enchantments/Imbued.h"
#include "game/Models/Enchantments/TezcatarasEmber.h"
#include "game/Models/Enchantments/SoulsPower.h"
#include "game/Models/Enchantments/Vigorous.h"
#include "core/dotnet_sort.h"

namespace sts2::CardCmd {

namespace {

void MoveToResultPileWithoutPlaying(PlayerChoiceContext& choiceContext, CardModel card) {
    CardPileCmd::Add(card, PileType::Play);
    card.MoveToResultPileWithoutPlaying(choiceContext);
}

// Pending AfflictionModel is a heap object in the reference game. Release it
// on every rejected or exceptional path until CardCmd.Afflict attaches it.
struct PendingAfflictionGuard {
    AfflictionModel& value;
    ~PendingAfflictionGuard() { value.ReleaseIfPending(); }
};

}  // namespace

void AutoPlay(PlayerChoiceContext& choiceContext, CardModel card, Creature target, AutoPlayType type, bool skipXCapture,
              bool skipCardPileVisuals) {
    CombatManager cm(&card.sim());
    if (cm.IsOverOrEnding() || card.Owner().Creature().IsDead()) return;
    CombatState combatState = card.CombatState() ? card.CombatState() : card.Owner().Creature().CombatState();
    if (card.Keywords().Contains(CardKeyword::Unplayable)) {
        MoveToResultPileWithoutPlaying(choiceContext, card);
        return;
    }
    AbstractModel preventer;
    if (!Hook::ShouldPlay(combatState, card, preventer, type)) {
        MoveToResultPileWithoutPlaying(choiceContext, card);
        return;  // the player dialogue line is presentation
    }
    RunRngSet& rng = card.Owner().RunState().Rng();
    if (card.TargetType() == TargetType::AnyEnemy) {
        if (!target) target = rng.CombatTargets().NextItem(combatState.HittableEnemies());
        if (!target) {
            MoveToResultPileWithoutPlaying(choiceContext, card);
            return;
        }
    }
    if (card.TargetType() == TargetType::AnyAlly) {
        CreatureList items;
        for (Creature c : combatState.Allies()) {
            if (c && c.IsAlive() && c.IsPlayer() && c != card.Owner().Creature()) items.push_back(c);
        }
        if (!target) target = rng.CombatTargets().NextItem(items);
        if (!target) {
            MoveToResultPileWithoutPlaying(choiceContext, card);
            return;
        }
    }
    PlayerCombatState playerCombatState = card.Owner().PlayerCombatState();
    if (card.EnergyCost().CostsX() && !skipXCapture) card.EnergyCost().SetCapturedXValue(playerCombatState.Energy());
    if (!skipXCapture) {
        if (card.HasStarCostX()) {
            card.SetLastStarsSpent(playerCombatState.Stars());
        } else {
            card.SetLastStarsSpent(std::max(0, card.GetStarCostWithModifiers()));
        }
    }
    if (!card.Pile()) CardPileCmd::Add(card, PileType::Play);
    Hook::BeforeCardAutoPlayed(combatState, card, target, type);
    ResourceInfo resources;
    resources.EnergySpent = 0;
    resources.EnergyValue = card.EnergyCost().GetAmountToSpend();
    resources.StarsSpent = 0;
    resources.StarValue = std::max(0, card.GetStarCostWithModifiers());
    card.OnPlayWrapper(choiceContext, target, /*isAutoPlay=*/true, resources, skipCardPileVisuals);
}

void Discard(PlayerChoiceContext& choiceContext, CardModel card) { Discard(choiceContext, CardList{card}); }

void Discard(PlayerChoiceContext& choiceContext, const CardList& cards) { DiscardAndDraw(choiceContext, cards, 0); }

void DiscardAndDraw(PlayerChoiceContext& choiceContext, const CardList& cardsToDiscard, int32_t cardsToDraw) {
    if (cardsToDiscard.empty()) return;
    CombatManager cm(&cardsToDiscard[0].sim());
    if (cm.IsOverOrEnding()) return;
    const CardList& discardCards = cardsToDiscard;
    CombatState combatState =
        discardCards[0].CombatState() ? discardCards[0].CombatState() : discardCards[0].Owner().Creature().CombatState();
    CardList slyCards;
    CardPile discardPile = CardPile::Get(PileType::Discard, discardCards[0].Owner());
    for (CardModel card : discardCards) {
        if (card.IsSlyThisTurn()) slyCards.push_back(card);
        CardPileCmd::Add(card, discardPile);
        cm.History().CardDiscarded(combatState, card);
        Hook::AfterCardDiscarded(combatState, choiceContext, card);
    }
    if (cardsToDraw > 0) CardPileCmd::Draw(choiceContext, Dec(cardsToDraw), discardCards[0].Owner());
    for (CardModel item : slyCards) AutoPlay(choiceContext, item, Creature{}, AutoPlayType::SlyDiscard);
}

std::optional<CardPileAddResult> Exhaust(PlayerChoiceContext& choiceContext, CardModel card, bool causedByEthereal,
                                         bool skipVisuals) {
    CombatManager cm(&card.sim());
    if (cm.IsOverOrEnding()) return std::nullopt;
    CombatState combatState = card.CombatState() ? card.CombatState() : card.Owner().Creature().CombatState();
    CardPileAddResult result = CardPileCmd::Add(card, PileType::Exhaust, CardPilePosition::Bottom, AbstractModel{}, skipVisuals);
    cm.History().CardExhausted(combatState, card);
    Hook::AfterCardExhausted(combatState, choiceContext, card, causedByEthereal);
    return result;
}

void Upgrade(CardModel card) { Upgrade(CardList{card}); }

void Upgrade(const CardList& cards) {
    if (cards.empty()) return;
    if (CombatManager(&cards[0].sim()).IsEnding()) return;
    for (CardModel card : cards) {
        if (!card.IsUpgradable()) continue;
        // CurrentMapPointHistoryEntry...UpgradedCards: run history only.
        card.UpgradeInternal();
        card.FinalizeUpgradeInternal();
    }
}

void Downgrade(CardModel card) {
    if (!card || CombatManager(&card.sim()).IsEnding()) return;
    card.DowngradeInternal();
}

void ApplyKeyword(CardModel card, std::initializer_list<CardKeyword> keywords) {
    for (CardKeyword keyword : keywords) card.AddKeyword(keyword);
}

void RemoveKeyword(CardModel card, std::initializer_list<CardKeyword> keywords) {
    for (CardKeyword keyword : keywords) card.RemoveKeyword(keyword);
}

void ApplySingleTurnSly(CardModel card) { card.GiveSingleTurnSly(); }
void ApplySingleTurnRetain(CardModel card) { card.GiveSingleTurnRetain(); }

bool CanEnchant(EnchantmentId enchantment, CardModel card) {
    if (!card || enchantment == EnchantmentId::NONE || enchantment >= EnchantmentId::Count) return false;

    const CardType type = card.Type();
    if (type == CardType::Status || type == CardType::Curse || type == CardType::Quest) return false;

    switch (enchantment) {
    case EnchantmentId::CORRUPTED:
    case EnchantmentId::INSTINCT:
    case EnchantmentId::MOMENTUM:
    case EnchantmentId::SHARP:
    case EnchantmentId::VIGOROUS:
        if (type != CardType::Attack) return false;
        break;
    case EnchantmentId::IMBUED:
        if (type != CardType::Skill) return false;
        break;
    case EnchantmentId::ROYALLY_APPROVED:
        if (type != CardType::Attack && type != CardType::Skill) return false;
        break;
    default:
        break;
    }

    CardPile pile = card.Pile();
    if (pile && pile.Type() == PileType::Deck && card.Keywords().Contains(CardKeyword::Unplayable)) return false;
    // EnchantmentModel.IsStackable is false for every source enchantment.
    if (card.Enchantment()) return false;

    switch (enchantment) {
    case EnchantmentId::GOOPY:
        return card.HasTag(CardTag::Defend);
    case EnchantmentId::NIMBLE:
        return card.GainsBlock();
    case EnchantmentId::SLITHER:
        return !card.Keywords().Contains(CardKeyword::Unplayable) && !card.EnergyCost().CostsX();
    case EnchantmentId::SOULS_POWER:
        return card.GetKeywordsWithSources(KeywordSources::Local).Contains(CardKeyword::Exhaust);
    case EnchantmentId::SPIRAL:
        return card.Rarity() == CardRarity::Basic &&
               (card.HasTag(CardTag::Strike) || card.HasTag(CardTag::Defend));
    default:
        return true;
    }
}

EnchantmentModel Enchant(EnchantmentId enchantment, CardModel card, Dec amount) {
    if (!card || enchantment == EnchantmentId::NONE) return {};
    if (!CanEnchant(enchantment, card)) {
        if (enchantment >= EnchantmentId::Count) throw GameError("Cannot enchant with an invalid enchantment id");
        throw GameError("Cannot enchant " + std::string(name(card.CardId_())) + " with " +
                        std::string(name(enchantment)) + ".");
    }
    card.st().enchantment = enchantment;
    card.st().enchantment_amount = static_cast<int32_t>(amount.to_int());
    card.st().enchantment_status = EnchantmentStatus::Normal;
    const auto vars = data(enchantment).vars;
    if (vars.size() > 1) throw Unported("Enchant: more than one canonical variable");
    card.st().enchantment_var = vars.empty() ? Dec(0) : vars.front().base;
    card.st().enchantment_extra[0] = card.st().enchantment_extra[1] = 0;
    card.Enchantment().ModifyCard();
    card.FinalizeUpgradeInternal();
    return card.Enchantment();
}

EnchantmentModel CloneEnchantment(CardModel source, CardModel target) {
    EnchantmentModel original = source.Enchantment();
    if (!original) return {};
    // The game clones the mutable EnchantmentModel, then CardCmd.Enchant
    // attaches it to the transformed card. Enchantment state is stored inside
    // CardState here, so retain its saved status and dynamic fields afterwards.
    EnchantmentModel clone = Enchant(original.EnchantmentId_(), target, Dec(original.Amount()));
    target.st().enchantment_status = source.st().enchantment_status;
    target.st().enchantment_var = source.st().enchantment_var;
    target.st().enchantment_extra[0] = source.st().enchantment_extra[0];
    target.st().enchantment_extra[1] = source.st().enchantment_extra[1];
    return clone;
}

// ---------------------------------------------------------------- afflictions

AfflictionModel Afflict(AfflictionId affliction, CardModel card, Dec amount) {
    return Afflict(AfflictionModel::Pending(&card.sim(), affliction), card, amount);  // ModelDb.Affliction<T>().ToMutable()
}

AfflictionModel Afflict(AfflictionModel affliction, CardModel card, Dec amount) {
    Sim* sim = &card.sim();
    PendingAfflictionGuard guard{affliction};
    if (CombatManager(sim).IsOverOrEnding()) {
        CardPile pile = card.Pile();
        if (pile && pile.IsCombatPile()) return AfflictionModel{};
    }
    CombatState combatState = card.CombatState();
    if (!combatState) combatState = card.Owner().Creature().CombatState();
    if (!combatState || !Hook::ShouldAfflict(combatState, card, affliction)) return AfflictionModel{};
    if (!affliction.CanAfflict(card)) return AfflictionModel{};
    if (!card.Affliction()) {
        AfflictionModel applied = card.AfflictInternal(affliction, amount);
        applied.AfterApplied();
    } else {
        if (card.Affliction().AfflictionId_() != affliction.AfflictionId_()) throw GameError("Cannot afflict a card that already has another affliction.");
        AfflictionModel current = card.Affliction();
        current.SetAmount(current.Amount() + amount.to_int());  // Amount += (int)amount
    }
    CombatManager(sim).History().CardAfflicted(combatState, card, card.Affliction());
    return card.Affliction();
}

std::vector<AfflictionModel> AfflictAndPreview(AfflictionId affliction, const CardList& cards, Dec amount) {
    std::vector<AfflictionModel> afflictions;
    for (CardModel card : cards) {
        AfflictionModel val = Afflict(affliction, card, amount);
        if (val) afflictions.push_back(val);
    }
    // Preview and Cmd.Wait: UI.
    return afflictions;
}

void ClearAffliction(CardModel card) { card.ClearAfflictionInternal(); }

// ---------------------------------------------------------------- transform

CardPileAddResult TransformToRandom(CardModel original, Rng& rng) {
    std::vector<CardTransformation> t;
    t.emplace_back(original);
    std::vector<CardPileAddResult> results = Transform(t, &rng);
    if (results.empty()) throw GameError("TransformToRandom: no result");  // .First()
    return results[0];
}

std::optional<CardPileAddResult> Transform(CardModel original, CardModel replacement) {
    std::vector<CardTransformation> t;
    t.emplace_back(original, replacement);
    std::vector<CardPileAddResult> results = Transform(t, nullptr);
    if (results.empty()) return std::nullopt;  // .FirstOrDefault()
    return results[0];
}

std::vector<CardPileAddResult> Transform(const std::vector<CardTransformation>& transformations, Rng* rng) {
    if (transformations.empty()) return {};
    Sim* sim = &transformations[0].Original.sim();
    if (CombatManager(sim).IsEnding()) return {};
    CombatState combatState = transformations[0].Original.CombatState();
    struct Data {
        size_t item;
        CardPile pile;
        int32_t index;
        CardModel replacement;
    };
    std::vector<Data> data;
    for (size_t i = 0; i < transformations.size(); ++i) {
        const CardTransformation& item = transformations[i];
        if (!item.Original.IsTransformable()) throw GameError("Can't transform a card because it's un-transformable.");
        CardPile pile = item.Original.Pile();
        if (!pile) throw GameError("Can't transform a card because it has no pile.");
        int32_t index = -1;
        CardList cards = pile.Cards();
        for (size_t k = 0; k < cards.size(); ++k) {
            if (cards[k] == item.Original) {
                index = static_cast<int32_t>(k);
                break;
            }
        }
        CardModel replacement = item.GetReplacement(rng);
        if (!replacement) throw GameError("Attempting to transform un-transformable card");
        item.Original.RemoveFromCurrentPile();
        data.push_back(Data{i, pile, index, replacement});
    }
    // transformationsWithOriginalData.Sort(PileIndexSort): .NET introsort.
    dotnet_sort(std::span<Data>(data.data(), data.size()), [](const Data& a, const Data& b) {
        if (a.pile.Type() != b.pile.Type()) return a.pile.Type() < b.pile.Type() ? -1 : 1;
        return a.index < b.index ? -1 : (a.index > b.index ? 1 : 0);
    });
    std::vector<CardPileAddResult> results;
    for (Data& d : data) {
        CardModel original = transformations[d.item].Original;
        RunState runState = original.Owner().RunState();
        CardModel replacement2 = d.replacement;
        CardPileAddResult result;
        result.success = true;
        result.cardAdded = replacement2;
        if (replacement2.Owner() != original.Owner()) throw GameError("the replacement has a different owner");
        if (d.pile.Type() == PileType::Deck) {
            ModelList modifyingModels;
            CardModel cardAdded = Hook::ModifyCardBeingAddedToDeck(runState, replacement2, modifyingModels);
            replacement2 = result.cardAdded = cardAdded;
            replacement2.SetFloorAddedToDeck(runState.TotalFloor());
            // CurrentMapPointHistoryEntry...CardsTransformed: run history only.
        }
        if (d.pile.Type() == PileType::Deck) {
            d.pile.AddInternal(replacement2);
        } else {
            d.pile.AddInternal(replacement2, d.index);
            CombatManager(sim).History().CardGenerated(combatState, replacement2, replacement2.Owner());
            Hook::AfterCardEnteredCombat(combatState, replacement2);
        }
        Hook::AfterCardChangedPiles(runState, combatState, replacement2, d.pile.Type(), AbstractModel{});
        // pile2.InvokeCardAddFinished(): the pile nodes' event.
        original.AfterTransformedFrom();
        replacement2.AfterTransformedTo();
        results.push_back(result);
    }
    // The VFX (NCardTransformVfx / hand shine) are skipped: TestMode or UI.
    for (size_t j = 0; j < results.size(); ++j) {
        const CardPileAddResult& r = results[j];
        if (r.success && r.cardAdded.Pile() && IsCombatPile(r.cardAdded.Pile().Type())) {
            Hook::AfterCardGeneratedForCombat(r.cardAdded.CombatState(), r.cardAdded, r.cardAdded.Owner());
        }
        transformations[data[j].item].Original.RemoveFromState();
    }
    return results;
}

}  // namespace sts2::CardCmd

namespace sts2 {

// Entities/Cards/CardTransformation.cs
CardTransformation::CardTransformation(CardModel original) : Original(original) {
    if (!original.IsTransformable()) throw GameError("Non-removable cards cannot be transformed!");
    IsInCombat = static_cast<bool>(original.CombatState());
}

CardTransformation::CardTransformation(CardModel original, std::vector<CardId> options)
    : Original(original), HasReplacementOptions(true), ReplacementOptions(std::move(options)) {
    if (!original.IsTransformable()) throw GameError("Non-removable cards cannot be transformed!");
    IsInCombat = static_cast<bool>(original.CombatState());
}

CardTransformation::CardTransformation(CardModel original, CardModel replacement) : Original(original), Replacement(replacement) {
    if (!original.IsTransformable()) throw GameError("Non-removable cards cannot be transformed!");
    IsInCombat = static_cast<bool>(original.CombatState());
}

CardModel CardTransformation::GetReplacement(Rng* rng) const {
    if (Replacement) return Replacement;
    if (rng == nullptr) throw GameError("RNG must be passed when replacement options is set!");
    if (!Original.IsTransformable()) return CardModel{};
    if (!HasReplacementOptions) return CardFactory::CreateRandomCardForTransform(Original, IsInCombat, *rng);
    return CardFactory::CreateRandomCardForTransform(Original, ReplacementOptions, IsInCombat, *rng);
}

}  // namespace sts2
