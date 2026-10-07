// Commands/CardPileCmd.cs
#include "game/Commands/CardPileCmd.h"

#include <algorithm>

#include "core/dotnet_sort.h"
#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Commands/CardCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Models/Canonical.h"

namespace sts2::CardPileCmd {

namespace {

// CheckIfDrawIsPossibleAndShowThoughtBubbleIfNot (the bubble is presentation).
bool CheckIfDrawIsPossible(Player player) {
    if (CardPile::Get(PileType::Draw, player).Count() + CardPile::Get(PileType::Discard, player).Count() == 0) return false;
    if (CardPile::Get(PileType::Hand, player).Count() >= CardPile::MaxCardsInHand) return false;
    return true;
}

}  // namespace

// ListExtensions.StableShuffle: List.Sort (CardModel.CompareTo), then UnstableShuffle.
void StableShuffle(CardList& list, Rng& rng) {
    dotnet_sort(list.span(), [](CardModel a, CardModel b) { return a.CompareTo(b); });
    rng.Shuffle(list);
}

CardModel AddCurseToDeck(CardId curse, Player owner) {
    StaticVec<CardId, 8> curses;
    curses.push_back(curse);
    CardPileAddResults results = AddCursesToDeck(curses, owner);
    return results.empty() ? CardModel{} : results[0].cardAdded;  // FirstOrDefault().cardAdded
}

CardPileAddResults AddCursesToDeck(const StaticVec<CardId, 8>& curses, Player owner) {
    CardPileAddResults results;
    for (CardId curse : curses) {
        CanonicalCard canonical(&owner.sim(), curse);
        if (canonical.get().Type() != CardType::Curse) throw GameError("AddCursesToDeck: not a curse");
        CardModel card = owner.RunState().CreateCard(curse, owner);
        results.push_back(Add(card, PileType::Deck));
    }
    // CardCmd.PreviewCardPileAdd(results, 2f): UI only.
    return results;
}

void RemoveFromDeck(CardModel card, bool showPreview) {
    CardList cards;
    cards.push_back(card);
    RemoveFromDeck(cards, showPreview);
}

void RemoveFromDeck(const CardList& cards, bool showPreview) {
    for (CardModel card : cards) {
        if (card.Pile().Type() != PileType::Deck) throw GameError("You cannot remove a card that is not in the deck.");
        // CurrentMapPointHistoryEntry...CardsRemoved: run history only.
        Hook::BeforeCardRemoved(card.Owner().RunState(), card);
        card.RemoveFromCurrentPile();
        (void)showPreview;  // NCard preview and VFX
        card.RemoveFromState();
    }
}

void RemoveFromCombat(CardModel card, bool skipVisuals) { RemoveFromCombat(CardList{card}, skipVisuals); }

void RemoveFromCombat(const CardList& cards, bool skipVisuals) {
    if (cards.empty()) return;
    CombatState combatState = cards[0].CombatState();
    RunState runState(&cards[0].sim());
    struct OldPile {
        CardModel card;
        CardPile pile;
    };
    StaticVec<OldPile, kMaxCardList> oldPiles;  // Dictionary<CardModel, CardPile>, no removals: insertion order
    for (CardModel card : cards) {
        CardPile value = card.Pile();
        if (!value || !value.IsCombatPile()) throw GameError("Card must be in a combat pile for it to be removed");
        for (const OldPile& o : oldPiles) {
            if (o.card == card) throw GameError("An item with the same key has already been added.");
        }
        oldPiles.push_back({card, value});
        card.RemoveFromCurrentPile();
    }
    for (const OldPile& item : oldPiles) {
        Hook::AfterCardChangedPiles(runState, combatState, item.card, item.pile.Type(), AbstractModel{});
        item.card.RemoveFromState();
    }
}

CardPileAddResult AddGeneratedCardToCombat(CardModel card, PileType newPileType, Player creator, CardPilePosition position) {
    CardPileAddResults results = AddGeneratedCardsToCombat(CardList{card}, newPileType, creator, position);
    if (results.empty()) throw GameError("AddGeneratedCardToCombat: no result was produced");
    return results[0];
}

CardPileAddResults AddGeneratedCardsToCombat(const CardList& cards, PileType newPileType, Player creator,
                                             CardPilePosition position) {
    CardPileAddResults results;
    if (cards.empty()) return results;
    CombatManager cm(&cards[0].sim());
    if (!cm.IsInProgress()) return results;
    for (CardModel c : cards) {
        if (c.Pile()) throw GameError("You are not allowed to generate cards that already have a pile");
    }
    if (!IsCombatPile(newPileType)) throw GameError("You are not allowed to added generated cards to a non combat pile");
    CombatState combatState = cards[0].Owner().Creature().CombatState();
    if (!combatState) return results;
    for (CardModel card : cards) {
        const int32_t generatedIndex = cm.History().CardGenerated(combatState, card, creator);
        results.push_back(Add(card, CardPile::Get(newPileType, card.Owner()), position));
        Hook::AfterCardGeneratedForCombat(combatState, card, creator);
        cm.History().CardGenerationResolved(card, generatedIndex);
    }
    return results;
}

CardPileAddResult Add(CardModel card, PileType newPileType, CardPilePosition position, AbstractModel clonedBy,
                      bool skipVisuals) {
    if (!card.Owner()) throw GameError("Attempted to add card to pile, but it has no owner!");
    return Add(card, CardPile::Get(newPileType, card.Owner()), position, clonedBy, skipVisuals);
}

CardPileAddResult Add(CardModel card, CardPile newPile, CardPilePosition position, AbstractModel clonedBy,
                      bool skipVisuals) {
    return Add(CardList{card}, newPile, position, clonedBy, skipVisuals)[0];
}

CardPileAddResults Add(const CardList& cards, PileType newPileType, CardPilePosition position, AbstractModel clonedBy,
                       bool skipVisuals) {
    if (cards.empty()) return {};
    return Add(cards, CardPile::Get(newPileType, cards[0].Owner()), position, clonedBy, skipVisuals);
}

CardPileAddResults Add(const CardList& cards, CardPile newPile, CardPilePosition position, AbstractModel clonedBy,
                       bool skipVisuals, bool isChangingOwners) {
    CardPileAddResults results;
    if (cards.empty()) return results;
    if (!newPile) throw GameError("CardPileCmd.Add: the target pile does not exist");
    Sim* sim = &newPile.sim();
    CombatManager cm(sim);
    RunState runState(sim);
    if (newPile.IsCombatPile() && cm.IsEnding()) {
        for (CardModel c : cards) results.push_back({.success = false, .cardAdded = c});
        return results;
    }
    Player player;
    for (CardModel card3 : cards) {
        if (!card3.Owner()) throw GameError("card has no owner.");
        Creature creature = card3.Owner().Creature();
        if (card3.HasBeenRemovedFromState() || creature.IsDead() || (card3.IsInCombat() && !creature.CombatState())) {
            results.push_back({.success = false, .cardAdded = card3, .oldPile = card3.Pile(), .targetPile = newPile.Type()});
            continue;
        }
        if (newPile.Type() == PileType::Deck) {
            if (!runState.ContainsCard(card3)) throw GameError("card must be added to a RunState before adding it to your deck.");
        } else if (card3.IsInCombat() && creature.CombatState() && !creature.CombatState().ContainsCard(card3)) {
            throw GameError("card must be added to a CombatState before adding it to this pile.");
        }
        // UpgradePreviewType.IsPreview(): previews are UI-only and never exist here.
        results.push_back({.success = true, .cardAdded = card3, .oldPile = card3.Pile(), .targetPile = newPile.Type()});
        if (!player) player = card3.Owner();
        if (player == card3.Owner()) continue;
        throw GameError("Tried to add cards with different owners to the same pile!");
    }
    if (newPile.Type() == PileType::Deck) {
        // Every result, including the ones that already failed.
        for (CardPileAddResult& result : results) {
            AbstractModel preventer;
            if (Hook::ShouldAddToDeck(runState, result.cardAdded, preventer)) {
                // CurrentMapPointHistoryEntry...CardsGained: run history only.
                result.cardAdded.SetFloorAddedToDeck(runState.TotalFloor());
            } else {
                preventer.AfterAddToDeckPrevented(result.cardAdded);
                result.success = false;
            }
        }
    }
    if (newPile.IsCombatPile() && !cm.IsInProgress()) return results;
    if (std::none_of(results.begin(), results.end(), [](const CardPileAddResult& r) { return r.success; })) return results;
    for (CardPileAddResult& value : results) {
        if (!value.success) continue;
        CardPile oldPile = value.oldPile;
        CardModel card = value.cardAdded;
        CardPile cardPile = newPile;
        bool isFullHandAdd = cardPile.Type() == PileType::Hand && cardPile.Count() >= CardPile::MaxCardsInHand;
        if (isFullHandAdd) cardPile = CardPile::Get(PileType::Discard, card.Owner());
        CardModel card2 = card;
        if (oldPile) {
            card.RemoveFromCurrentPile(skipVisuals);
        } else if (cardPile.Type() == PileType::Deck) {
            ModelList modifyingModels;
            CardModel cardModel = Hook::ModifyCardBeingAddedToDeck(runState, card, modifyingModels);
            card2 = cardModel;
            if (!modifyingModels.empty()) value.cardAdded = cardModel;
        }
        int32_t index;
        switch (position) {
        case CardPilePosition::Bottom: index = -1; break;
        case CardPilePosition::Top: index = 0; break;
        case CardPilePosition::Random: index = runState.Rng().Shuffle().NextInt(cardPile.Count() + 1); break;
        default: throw GameError("position out of range");
        }
        cardPile.AddInternal(card2, index);
        if (!oldPile && cardPile.IsCombatPile() && !isChangingOwners) Hook::AfterCardEnteredCombat(card.CombatState(), card);
    }
    // The tween (GetTweenForCardsChangingPiles) is null in TestMode, so this always runs.
    for (const CardPileAddResult& item4 : results) {
        if (!item4.success) continue;
        CardModel cardAdded = item4.cardAdded;
        CardPile pile = cardAdded.Pile();
        if (!item4.oldPile || !pile || item4.oldPile.Type() != pile.Type()) {
            Hook::AfterCardChangedPiles(runState, cardAdded.CombatState(), cardAdded,
                                        item4.oldPile ? item4.oldPile.Type() : PileType::None, clonedBy);
        }
    }
    return results;
}

void AddDuringManualCardPlay(CardModel card) {
    CombatManager cm(&card.sim());
    if (cm.IsOverOrEnding()) return;
    CombatState combatState = card.Owner().Creature().CombatState();
    if (!combatState || !combatState.ContainsCard(card)) throw GameError("card must be added to a CombatState before playing it.");
    CardPile oldPile = card.Pile();
    card.RemoveFromCurrentPile();
    CardPile::Get(PileType::Play, card.Owner()).AddInternal(card);
    // No card node in TestMode, so no tween to wait on (even for powers).
    Hook::AfterCardChangedPiles(RunState(&card.sim()), card.CombatState(), card, oldPile ? oldPile.Type() : PileType::None,
                                AbstractModel{});
}

CardModel Draw(PlayerChoiceContext& choiceContext, Player player) {
    CardList drawn = Draw(choiceContext, Dec(1), player);
    return drawn.empty() ? CardModel{} : drawn[0];
}

// DrawInternal
CardList Draw(PlayerChoiceContext& choiceContext, Dec count, Player player, bool fromHandDraw) {
    CardList result;
    CombatManager cm(&player.sim());
    if (cm.IsOverOrEnding()) return result;
    AbstractModel modifier;
    if (!Hook::ShouldDraw(player.Creature().CombatState(), player, fromHandDraw, modifier)) {
        Hook::AfterPreventingDraw(player.Creature().CombatState(), modifier);
        return result;
    }
    CombatState combatState = player.Creature().CombatState();
    CardPile hand = CardPile::Get(PileType::Hand, player);
    CardPile drawPile = CardPile::Get(PileType::Draw, player);
    int32_t drawsRequested = count > Dec(0) ? dec_ceiling(count).to_int() : 0;
    if (drawsRequested == 0) return result;
    int32_t num = std::max(0, CardPile::MaxCardsInHand - hand.Count());
    if (num == 0) return result;
    for (int32_t i = 0; i < drawsRequested; ++i) {
        if (num <= 0) break;
        if (cm.IsOverOrEnding()) break;
        if (!CheckIfDrawIsPossible(player)) break;
        ShuffleIfNecessary(choiceContext, player);
        if (!CheckIfDrawIsPossible(player)) break;
        CardModel card = drawPile.IsEmpty() ? CardModel{} : CardModel(&player.sim(), drawPile.refs()[0]);
        if (!card || hand.Count() >= CardPile::MaxCardsInHand) break;
        result.push_back(card);
        Add(card, hand);
        const int32_t drawnIndex = cm.History().CardDrawn(combatState, card, fromHandDraw);
        Hook::AfterCardDrawn(combatState, choiceContext, card, fromHandDraw);
        card.InvokeDrawn();
        cm.History().CardDrawResolved(card, drawnIndex);
        num = std::max(0, CardPile::MaxCardsInHand - hand.Count());
    }
    return result;
}

void Shuffle(PlayerChoiceContext& choiceContext, Player player) {
    CombatManager cm(&player.sim());
    if (cm.IsOverOrEnding()) return;
    CardPile pile = CardPile::Get(PileType::Draw, player);
    CardList list = CardPile::Get(PileType::Discard, player).Cards();
    // HashSet built from the pile (no removals): enumerates in pile order.
    CardList drawPileCards = pile.Cards();
    for (CardModel c : drawPileCards) list.push_back(c);
    StableShuffle(list, player.RunState().Rng().Shuffle());
    Hook::ModifyShuffleOrder(player.Creature().CombatState(), player, list, /*isInitialShuffle=*/false);
    // DebugForcedTopCardOnNextShuffle is a debug-console feature (always null).
    Add(list, pile, CardPilePosition::Bottom, AbstractModel{}, /*skipVisuals=*/true);
    // Tweens (none in TestMode) and waits.
    if (!cm.IsOverOrEnding()) Hook::AfterShuffle(player.Creature().CombatState(), choiceContext, player);
}

void ShuffleIfNecessary(PlayerChoiceContext& choiceContext, Player player) {
    CardPile pile = CardPile::Get(PileType::Draw, player);
    CardPile pile2 = CardPile::Get(PileType::Discard, player);
    if (pile.IsEmpty() && !pile2.IsEmpty()) {
        // ShuffleFtueCheck: the FTUE is marked seen.
        Shuffle(choiceContext, player);
    }
}

void AddToCombatAndPreview(CardId card, const CreatureList& targets, PileType pileType, int32_t count, Player creator,
                           CardPilePosition position) {
    for (Creature target : targets) AddToCombatAndPreview(card, target, pileType, count, creator, position);
}

void AddToCombatAndPreview(CardId card, Creature target, PileType pileType, int32_t count, Player creator,
                           CardPilePosition position) {
    Player player = target.Player() ? target.Player() : target.PetOwner();
    if (player.Creature().IsDead()) return;
    for (int32_t i = 0; i < count; ++i) {
        CombatState combatState = target.CombatState();
        CardModel cardModel = combatState ? combatState.CreateCard(card, player) : CardModel{};
        if (cardModel) AddGeneratedCardToCombat(cardModel, pileType, creator, position);
    }
    // The preview and its waits are presentation.
}

void AutoPlayFromDrawPile(PlayerChoiceContext& choiceContext, Player player, int32_t count,
                          CardPilePosition position, bool forceExhaust) {
    if (count <= 0) return;
    CardPile draw = CardPile::Get(PileType::Draw, player);
    CardList cards;
    for (int32_t i = 0; i < count; ++i) {
        ShuffleIfNecessary(choiceContext, player);
        CardList pileCards = draw.Cards();
        if (pileCards.empty()) break;
        CardModel card = position == CardPilePosition::Top ? pileCards.front() : pileCards.back();
        cards.push_back(card);
        Add(card, PileType::Play, CardPilePosition::Bottom, AbstractModel{}, true);
    }
    for (CardModel card : cards) {
        if (forceExhaust) card.SetExhaustOnNextPlay(true);
        CardCmd::AutoPlay(choiceContext, card, Creature{}, AutoPlayType::Default,
                          /*skipXCapture=*/false, /*skipCardPileVisuals=*/true);
    }
}

}  // namespace sts2::CardPileCmd
