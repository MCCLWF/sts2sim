// Commands/CardSelectCmd.cs and CardSelection/CardSelectorPrefs.cs.
//
// Every selection screen is a player choice (Sim::Choose). The options are
// presented in the order the game hands them to a card selector
// (CardSelectCmd.Selector, which the oracle drives). A selection of several
// cards is an ordered subset: the first answer is how many cards, then one
// answer per card index, in selection order (the game processes the selected
// cards in that order). A single choice (FromChooseACardScreen, a card reward)
// is one answer.
#pragma once

#include <string_view>
#include <vector>

#include "game/All.h"

namespace sts2 {

struct CardSelectorPrefs {
    int32_t MinSelect = 0;
    int32_t MaxSelect = 0;
    bool RequireManualConfirmation = false;
    bool Cancelable = false;

    CardSelectorPrefs() = default;
    explicit CardSelectorPrefs(int32_t selectCount) : CardSelectorPrefs(selectCount, selectCount) {}
    CardSelectorPrefs(int32_t minCount, int32_t maxCount)
        : MinSelect(minCount), MaxSelect(maxCount), RequireManualConfirmation(minCount >= 0 && minCount != maxCount) {}
};

using CardFilter = bool (*)(CardModel card);

namespace CardSelectCmd {

// Temporarily select the first available cards without consuming player-choice
// answers. The active policy is stored in GameState so scope behavior follows
// snapshots and participates in state hashing.
class ScopedDeterministicSelector {
public:
    explicit ScopedDeterministicSelector(Sim* sim) noexcept;
    ~ScopedDeterministicSelector();
    ScopedDeterministicSelector(const ScopedDeterministicSelector&) = delete;
    ScopedDeterministicSelector& operator=(const ScopedDeterministicSelector&) = delete;
    ScopedDeterministicSelector(ScopedDeterministicSelector&&) = delete;
    ScopedDeterministicSelector& operator=(ScopedDeterministicSelector&&) = delete;

private:
    GameState* state_;
    CardSelectorMode previous_;
};

bool FirstSelectorActive(const Sim* sim) noexcept;

// The selection of `count` cards among `options` (the selector step shared by
// the screens below); `source` names the screen for agents and traces.
CardList Select(Sim* sim, const CardList& options, int32_t minSelect, int32_t maxSelect, std::string_view source);

CardModel FromChooseACardScreen(PlayerChoiceContext& context, const CardList& cards, Player player, bool canSkip = false);
CardList FromSimpleGridForRewards(PlayerChoiceContext& context, const CardList& cards, Player player, CardSelectorPrefs prefs);
CardList FromSimpleGrid(PlayerChoiceContext& context, const CardList& cards, Player player, CardSelectorPrefs prefs);
CardList FromCombatPile(PlayerChoiceContext& context, CardPile pile, Player player, CardSelectorPrefs prefs,
                        CardFilter filter = nullptr);
CardList FromCombatPile(PlayerChoiceContext& context, CardPile pile, Player player, CardSelectorPrefs prefs,
                        const CardList& allowedCards);
CardList FromHand(PlayerChoiceContext& context, Player player, CardSelectorPrefs prefs, CardFilter filter, AbstractModel source);
CardList FromHandForDiscard(PlayerChoiceContext& context, Player player, CardSelectorPrefs prefs, CardFilter filter,
                            AbstractModel source);
CardModel FromHandForUpgrade(PlayerChoiceContext& context, Player player, AbstractModel source);
CardList FromDeckForUpgrade(Player player, CardSelectorPrefs prefs);
CardList FromDeckForTransformation(Player player, CardSelectorPrefs prefs);
CardList FromDeckForEnchantment(Player player, CardSelectorPrefs prefs, CardFilter filter);
CardList FromDeckGeneric(Player player, CardSelectorPrefs prefs, CardFilter filter = nullptr);
// FromDeckForRemoval(player, prefs, filter): the removable cards, curses first
// (OrderBy: curses at -999999999, the rest by deck index).
CardList FromDeckForRemoval(Player player, CardSelectorPrefs prefs, CardFilter filter = nullptr);
// FromChooseABundleScreen(player, bundles): one choice (kind Bundle, answer =
// bundle index). The game's TestMode takes bundle 0; the oracle's patch
// (OracleRun) asks its policy like the client's screen asks the player.
CardList FromChooseABundleScreen(Player player, const std::vector<CardList>& bundles);

}  // namespace CardSelectCmd
}  // namespace sts2
