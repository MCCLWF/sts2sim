// Commands/CardSelectCmd.cs (the selector paths; screens and network sync
// are the UI's).
#include "game/Commands/CardSelectCmd.h"

#include <algorithm>

#include "game/Combat/CombatManager.h"

namespace sts2::CardSelectCmd {

ScopedDeterministicSelector::ScopedDeterministicSelector(Sim* sim) noexcept
    : state_(sim->g), previous_(sim->g->card_selector_mode) {
    state_->card_selector_mode = CardSelectorMode::FirstN;
}

ScopedDeterministicSelector::~ScopedDeterministicSelector() {
    state_->card_selector_mode = previous_;
}

bool FirstSelectorActive(const Sim* sim) noexcept {
    return sim->g->card_selector_mode == CardSelectorMode::FirstN;
}

namespace {

// UndoEndTurnIfNecessary: a selection during the player's turn takes back a
// pending end of turn (CombatManager.UndoReadyToEndTurn).
void UndoEndTurnIfNecessary(Player player) {
    CombatData& c = player.sim().g->combat;
    CombatState state = player.Creature().CombatState();
    if (c.player_ready_to_end_turn && state && state.CurrentSide() == CombatSide::Player) {
        c.player_ready_to_end_turn = false;
        c.player_actions_disabled = false;
    }
}

CardList Filter(const CardList& cards, CardFilter filter) {
    if (filter == nullptr) return cards;
    CardList out;
    for (CardModel c : cards) {
        if (filter(c)) out.push_back(c);
    }
    return out;
}

CardList SelectFromCombatPileOptions(PlayerChoiceContext&, CardPile pile, Player player, CardSelectorPrefs prefs,
                                     CardList enumerable) {
    int32_t num = static_cast<int32_t>(enumerable.size());
    if (num == 0) return {};
    if (!prefs.RequireManualConfirmation && num <= prefs.MinSelect) return enumerable;
    if (pile.Type() == PileType::Draw) {
        // The draw pile is shown sorted: OrderBy(Rarity).ThenBy(Id) (stable).
        std::stable_sort(enumerable.begin(), enumerable.end(), [](CardModel a, CardModel b) {
            CardRarity ra = a.Rarity(), rb = b.Rarity();
            if (ra != rb) return ra < rb;
            return a.data().entry < b.data().entry;
        });
    }
    return Select(&player.sim(), enumerable, prefs.MinSelect, prefs.MaxSelect, "FromCombatPile");
}

}  // namespace

CardList Select(Sim* sim, const CardList& options, int32_t minSelect, int32_t maxSelect, std::string_view source) {
    if (FirstSelectorActive(sim)) {
        const int32_t count = std::min(maxSelect, static_cast<int32_t>(options.size()));
        CardList result;
        for (int32_t i = 0; i < count; ++i) result.push_back(options[static_cast<size_t>(i)]);
        return result;
    }

    ChoiceRequest request;
    request.kind = ChoiceKind::CardSelect;
    request.option_count = static_cast<int32_t>(options.size());
    request.min_select = minSelect;
    request.max_select = maxSelect;
    request.subset = true;
    request.can_skip = minSelect == 0;
    request.source = std::string(source);
    request.prompt = "count";
    for (CardModel c : options) {
        request.cards.push_back(c.Index());
        request.options.push_back({.kind = ChoiceOptionKind::Card, .cards = {c.Index()}});
    }
    int32_t count = sim->Choose(request);
    int32_t most = std::min(maxSelect, static_cast<int32_t>(options.size()));
    if (count < 0 || count > most || count < std::min(minSelect, most)) throw GameError("CardSelectCmd: selection count out of range");
    CardList result;
    for (int32_t k = 0; k < count; ++k) {
        request.prompt = "index";
        request.selected.clear();
        for (CardModel c : result) {
            for (size_t j = 0; j < options.size(); ++j) {
                if (options[j] == c) request.selected.push_back(static_cast<int32_t>(j));
            }
        }
        int32_t i = sim->Choose(request);
        if (i < 0 || i >= static_cast<int32_t>(options.size())) throw GameError("CardSelectCmd: card index out of range");
        CardModel card = options[static_cast<size_t>(i)];
        if (result.contains(card)) throw GameError("CardSelectCmd: a card selected twice");
        result.push_back(card);
    }
    return result;
}

CardModel FromChooseACardScreen(PlayerChoiceContext& context, const CardList& cards, Player player, bool canSkip) {
    if (cards.size() > 3) throw GameError("Only works with less than 3 cards");
    if (cards.empty()) return CardModel{};  // ReportSoftlock
    UndoEndTurnIfNecessary(player);
    CardList result = Select(&player.sim(), cards, 0, 1, "FromChooseACardScreen");  // Selector.GetSelectedCards(cards, 0, 1)
    return result.empty() ? CardModel{} : result[0];
}

CardList FromSimpleGridForRewards(PlayerChoiceContext& context, const CardList& cards, Player player, CardSelectorPrefs prefs) {
    if (CombatManager(&player.sim()).IsEnding()) return {};
    UndoEndTurnIfNecessary(player);
    if (cards.empty()) return {};  // ReportSoftlock
    if (!prefs.RequireManualConfirmation && static_cast<int32_t>(cards.size()) <= prefs.MinSelect) return cards;
    return Select(&player.sim(), cards, prefs.MinSelect, prefs.MaxSelect, "FromSimpleGridForRewards");
}

CardList FromSimpleGrid(PlayerChoiceContext& context, const CardList& cards, Player player, CardSelectorPrefs prefs) {
    if (CombatManager(&player.sim()).IsEnding()) return {};
    UndoEndTurnIfNecessary(player);
    if (cards.empty()) return {};
    if (!prefs.RequireManualConfirmation && static_cast<int32_t>(cards.size()) <= prefs.MinSelect) return cards;
    return Select(&player.sim(), cards, prefs.MinSelect, prefs.MaxSelect, "FromSimpleGrid");
}

CardList FromCombatPile(PlayerChoiceContext& context, CardPile pile, Player player, CardSelectorPrefs prefs, CardFilter filter) {
    if (CombatManager(&player.sim()).IsEnding()) return {};
    if (!pile.IsCombatPile()) throw GameError("Cannot perform on a non combat pile");
    UndoEndTurnIfNecessary(player);
    CardList enumerable = Filter(pile.Cards(), filter);
    return SelectFromCombatPileOptions(context, pile, player, prefs, enumerable);
}

CardList FromCombatPile(PlayerChoiceContext& context, CardPile pile, Player player, CardSelectorPrefs prefs,
                        const CardList& allowedCards) {
    if (CombatManager(&player.sim()).IsEnding()) return {};
    if (!pile.IsCombatPile()) throw GameError("Cannot perform on a non combat pile");
    UndoEndTurnIfNecessary(player);
    CardList enumerable;
    for (CardModel card : pile.Cards()) {
        if (allowedCards.contains(card)) enumerable.push_back(card);
    }
    return SelectFromCombatPileOptions(context, pile, player, prefs, enumerable);
}

CardList FromHand(PlayerChoiceContext& context, Player player, CardSelectorPrefs prefs, CardFilter filter, AbstractModel source) {
    if (CombatManager(&player.sim()).IsOverOrEnding()) return {};
    UndoEndTurnIfNecessary(player);
    CardList list = Filter(player.PlayerCombatState().Hand().Cards(), filter);
    if (list.empty()) return list;
    if (!prefs.RequireManualConfirmation && static_cast<int32_t>(list.size()) <= prefs.MinSelect) return list;
    return Select(&player.sim(), list, prefs.MinSelect, prefs.MaxSelect, "FromHand");
}

CardList FromHandForDiscard(PlayerChoiceContext& context, Player player, CardSelectorPrefs prefs, CardFilter filter,
                            AbstractModel source) {
    // ShouldGlowGold: display only.
    return FromHand(context, player, prefs, filter, source);
}

CardModel FromHandForUpgrade(PlayerChoiceContext& context, Player player, AbstractModel source) {
    if (CombatManager(&player.sim()).IsOverOrEnding()) return CardModel{};
    UndoEndTurnIfNecessary(player);
    CardList list;
    for (CardModel c : player.PlayerCombatState().Hand().Cards()) {
        if (c.IsUpgradable()) list.push_back(c);
    }
    if (list.size() <= 1) return list.empty() ? CardModel{} : list[0];
    CardList result = Select(&player.sim(), list, 1, 1, "FromHandForUpgrade");
    return result.empty() ? CardModel{} : result[0];
}

CardList FromDeckForUpgrade(Player player, CardSelectorPrefs prefs) {
    CardList list;
    for (CardModel c : player.Deck().Cards()) {
        if (c.IsUpgradable()) list.push_back(c);
    }
    if (list.empty()) return {};
    if (static_cast<int32_t>(list.size()) <= prefs.MinSelect && !prefs.RequireManualConfirmation) return list;
    return Select(&player.sim(), list, prefs.MinSelect, prefs.MaxSelect, "FromDeckForUpgrade");
}

CardList FromDeckForTransformation(Player player, CardSelectorPrefs prefs) {
    CardList list;
    for (CardModel c : player.Deck().Cards()) {
        if (c.Type() != CardType::Quest && c.IsTransformable()) list.push_back(c);
    }
    if (list.empty()) return {};
    if (static_cast<int32_t>(list.size()) <= prefs.MinSelect && !prefs.RequireManualConfirmation) return list;
    return Select(&player.sim(), list, prefs.MinSelect, prefs.MaxSelect, "FromDeckForTransformation");
}

CardList FromDeckForEnchantment(Player player, CardSelectorPrefs prefs, CardFilter filter) {
    CardList list = Filter(player.Deck().Cards(), filter);
    if (list.empty()) return {};
    if (static_cast<int32_t>(list.size()) <= prefs.MinSelect) return list;
    return Select(&player.sim(), list, prefs.MinSelect, prefs.MaxSelect, "FromDeckForEnchantment");
}

CardList FromDeckGeneric(Player player, CardSelectorPrefs prefs, CardFilter filter) {
    CardList list = Filter(player.Deck().Cards(), filter);
    if (player.Creature().IsDead()) return {};
    // sortingOrder: not ported (no caller passes one yet).
    if (list.empty()) return {};
    if (!prefs.RequireManualConfirmation && static_cast<int32_t>(list.size()) <= prefs.MinSelect) return list;
    return Select(&player.sim(), list, prefs.MinSelect, prefs.MaxSelect, "FromDeckGeneric");
}

CardList FromDeckForRemoval(Player player, CardSelectorPrefs prefs, CardFilter filter) {
    // FromDeckGeneric(player, prefs, c => c.IsRemovable && filter(c), sortingOrder)
    CardList deck = player.Deck().Cards();
    CardList list;
    for (CardModel c : deck) {
        if (c.IsRemovable() && (filter == nullptr || filter(c))) list.push_back(c);
    }
    if (player.Creature().IsDead()) return {};
    // list.OrderBy(sortingOrder): a stable sort by the key.
    auto key = [&](CardModel c) -> int64_t {
        if (c.Type() == CardType::Curse) return -999999999;
        for (size_t i = 0; i < deck.size(); ++i) {
            if (deck[i] == c) return static_cast<int64_t>(i);
        }
        return -1;
    };
    std::stable_sort(list.begin(), list.end(), [&](CardModel a, CardModel b) { return key(a) < key(b); });
    if (list.empty()) return {};
    if (!prefs.RequireManualConfirmation && static_cast<int32_t>(list.size()) <= prefs.MinSelect) return list;
    return Select(&player.sim(), list, prefs.MinSelect, prefs.MaxSelect, "FromDeckGeneric");
}

CardList FromChooseABundleScreen(Player player, const std::vector<CardList>& bundles) {
    if (CombatManager(&player.sim()).IsEnding()) return {};
    if (bundles.empty()) return {};  // ReportSoftlock
    ChoiceRequest request;
    request.kind = ChoiceKind::Bundle;
    request.option_count = static_cast<int32_t>(bundles.size());
    request.source = "FromChooseABundleScreen";
    request.prompt = "bundle";
    for (const CardList& b : bundles) {
        ChoiceOption option{.kind = ChoiceOptionKind::Bundle};
        for (CardModel c : b) request.cards.push_back(c.Index());
        for (CardModel c : b) option.cards.push_back(c.Index());
        request.options.push_back(std::move(option));
    }
    int32_t i = player.sim().Choose(request);
    if (i < 0 || i >= static_cast<int32_t>(bundles.size())) throw GameError("FromChooseABundleScreen: bundle index out of range");
    return bundles[static_cast<size_t>(i)];
}

}  // namespace sts2::CardSelectCmd
