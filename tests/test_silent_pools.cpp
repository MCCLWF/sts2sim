#include <algorithm>

#include "engine/run_setup.h"
#include "game/Factories/CardFactory.h"
#include "game/Factories/PotionFactory.h"

using namespace sts2;

namespace {

template <class List, class Id>
bool all_absent(const List& list, std::initializer_list<Id> ids) {
    for (Id id : ids) if (list.contains(id)) return false;
    return true;
}

template <class List, class Id>
bool all_present(const List& list, std::initializer_list<Id> ids) {
    for (Id id : ids) if (!list.contains(id)) return false;
    return true;
}

}  // namespace

int main() {
    constexpr uint64_t all_epochs = 0x1ffffffffffffffULL;
    constexpr uint64_t gated_epochs = (uint64_t{1} << 50) | (uint64_t{1} << 51) | (uint64_t{1} << 52) |
                                      (uint64_t{1} << 53) | (uint64_t{1} << 54) | (uint64_t{1} << 55);
    GameState state;
    Sim sim(state);
    NewRunOptions options{.seed = "SILENTPOOLLOCKS",
                          .character = CharacterId::SILENT,
                          .ascension = 0,
                          .profile = ProfileConfig{.all_unlocked = true, .unlocked_epoch_bits = all_epochs & ~gated_epochs}};
    new_run(state, options);
    Player player(&sim, 1);

    if (state.run.profile.unlocked_epoch_bits != state.run.player.profile.unlocked_epoch_bits ||
        state.run.profile.full_veteran_compatible) return 1;

    const auto cards = CardFactory::GetUnlockedCards(CardPoolId::SILENT_CARD_POOL, player);
    if (!all_absent(cards, {CardId::SNAKEBITE, CardId::BUBBLE_BUBBLE, CardId::ACCELERANT,
                            CardId::REFLEX, CardId::MASTER_PLANNER, CardId::HAND_TRICK,
                            CardId::HIDDEN_DAGGERS, CardId::BLADE_OF_INK, CardId::PHANTOM_BLADES})) return 2;
    if (!cards.contains(CardId::MURDER) || !cards.contains(CardId::WRAITH_FORM)) return 3;
    if (!all_absent(cards, {CardId::CONCOCT, CardId::FADE, CardId::FLANKING, CardId::BLADE_SYMPHONY, CardId::SNEAKY})) return 4;

    const auto potions = PotionFactory::GetPotionOptions(player);
    if (!all_absent(potions, {PotionId::POISON_POTION, PotionId::GHOST_IN_A_JAR, PotionId::CUNNING_POTION})) return 5;
    if (potions.empty()) return 6;  // shared potions remain available

    const auto& shared = state.run.shared_relic_grab_bag.original;
    const auto& character = state.run.player.relic_grab_bag.original;
    const RelicId silent3[] = {RelicId::PAPER_KRANE, RelicId::TINGSHA, RelicId::TOUGH_BANDAGES};
    const RelicId silent6[] = {RelicId::TWISTED_FUNNEL, RelicId::SNECKO_SKULL, RelicId::HELICAL_DART};
    if (!all_absent(shared, {silent3[0], silent3[1], silent3[2], silent6[0], silent6[1], silent6[2]})) return 7;
    if (!all_absent(character, {silent3[0], silent3[1], silent3[2], silent6[0], silent6[1], silent6[2]})) return 8;
    if (!character.contains(RelicId::NINJA_SCROLL)) return 9;

    // Exercise each source FilterThroughEpochs predicate independently. The
    // other Silent unlock bits stay enabled so an incorrect epoch mapping
    // cannot pass merely because every gated bit is clear.
    const struct {
        uint8_t epoch;
        CardId cards[3];
    } cardGates[] = {
        {50, {CardId::SNAKEBITE, CardId::BUBBLE_BUBBLE, CardId::ACCELERANT}},
        {53, {CardId::REFLEX, CardId::MASTER_PLANNER, CardId::HAND_TRICK}},
        {55, {CardId::HIDDEN_DAGGERS, CardId::BLADE_OF_INK, CardId::PHANTOM_BLADES}},
    };
    for (const auto& gate : cardGates) {
        GameState gateState;
        Sim gateSim(gateState);
        const uint64_t mask = all_epochs & ~(uint64_t{1} << gate.epoch);
        new_run(gateState, NewRunOptions{.seed = "SILENTCARDGATE", .character = CharacterId::SILENT,
                                        .ascension = 0,
                                        .profile = ProfileConfig{.all_unlocked = true, .unlocked_epoch_bits = mask}});
        const auto gatedCards = CardFactory::GetUnlockedCards(CardPoolId::SILENT_CARD_POOL, Player(&gateSim, 1));
        if (!all_absent(gatedCards, {gate.cards[0], gate.cards[1], gate.cards[2]})) return 14;
        for (const auto& other : cardGates) {
            if (other.epoch == gate.epoch) continue;
            if (!all_present(gatedCards, {other.cards[0], other.cards[1], other.cards[2]})) return 15;
        }
    }

    const struct {
        uint8_t epoch;
        RelicId relics[3];
    } relicGates[] = {
        {51, {RelicId::PAPER_KRANE, RelicId::TINGSHA, RelicId::TOUGH_BANDAGES}},
        {54, {RelicId::TWISTED_FUNNEL, RelicId::SNECKO_SKULL, RelicId::HELICAL_DART}},
    };
    for (const auto& gate : relicGates) {
        GameState gateState;
        Sim gateSim(gateState);
        const uint64_t mask = all_epochs & ~(uint64_t{1} << gate.epoch);
        new_run(gateState, NewRunOptions{.seed = "SILENTRELICGATE", .character = CharacterId::SILENT,
                                        .ascension = 0,
                                        .profile = ProfileConfig{.all_unlocked = true, .unlocked_epoch_bits = mask}});
        const auto& gatedBag = gateState.run.player.relic_grab_bag.original;
        if (!all_absent(gatedBag, {gate.relics[0], gate.relics[1], gate.relics[2]})) return 16;
        const auto& other = relicGates[gate.epoch == 51 ? 1 : 0];
        if (!all_present(gatedBag, {other.relics[0], other.relics[1], other.relics[2]})) return 17;
    }

    {
        GameState potionState;
        Sim potionSim(potionState);
        const uint64_t mask = all_epochs & ~(uint64_t{1} << 52);
        new_run(potionState, NewRunOptions{.seed = "SILENTPOTIONGATE", .character = CharacterId::SILENT,
                                           .ascension = 0,
                                           .profile = ProfileConfig{.all_unlocked = true, .unlocked_epoch_bits = mask}});
        const auto gatedPotions = PotionFactory::GetPotionOptions(Player(&potionSim, 1));
        if (!all_absent(gatedPotions, {PotionId::POISON_POTION, PotionId::GHOST_IN_A_JAR,
                                      PotionId::CUNNING_POTION})) return 18;
    }

    // Multiplayer constraints are table-driven source overrides and apply in
    // the same pool path as Silent's epoch filters.
    for (CardId id : {CardId::CONCOCT, CardId::FADE, CardId::FLANKING, CardId::BLADE_SYMPHONY, CardId::SNEAKY}) {
        CardModel card = player.RunState().CreateCard(id, player);
        if (card.MultiplayerConstraint() != CardMultiplayerConstraint::MultiplayerOnly) return 10;
    }

    GameState fullState;
    Sim fullSim(fullState);
    new_run(fullState, NewRunOptions{.seed = "SILENTPOOLFULL", .character = CharacterId::SILENT, .ascension = 0});
    Player fullPlayer(&fullSim, 1);
    const auto fullCards = CardFactory::GetUnlockedCards(CardPoolId::SILENT_CARD_POOL, fullPlayer);
    size_t expectedSingleplayerCards = 0;
    for (CardId id : data(CardPoolId::SILENT_CARD_POOL).cards) {
        const bool multiplayerOnly = data(id).multiplayer == CardMultiplayerConstraint::MultiplayerOnly;
        if (multiplayerOnly) {
            if (fullCards.contains(id)) return 19;
        } else {
            ++expectedSingleplayerCards;
            if (!fullCards.contains(id)) return 20;
        }
    }
    if (fullCards.size() != expectedSingleplayerCards) return 21;
    if (!all_present(fullCards, {CardId::SNAKEBITE, CardId::BUBBLE_BUBBLE, CardId::ACCELERANT,
                                 CardId::REFLEX, CardId::MASTER_PLANNER, CardId::HAND_TRICK,
                                 CardId::HIDDEN_DAGGERS, CardId::BLADE_OF_INK, CardId::PHANTOM_BLADES})) return 11;
    const auto fullPotions = PotionFactory::GetPotionOptions(fullPlayer);
    if (!all_present(fullPotions, {PotionId::POISON_POTION, PotionId::GHOST_IN_A_JAR, PotionId::CUNNING_POTION})) return 12;
    if (!all_present(fullState.run.player.relic_grab_bag.original,
                     {RelicId::PAPER_KRANE, RelicId::TINGSHA, RelicId::TOUGH_BANDAGES,
                      RelicId::TWISTED_FUNNEL, RelicId::SNECKO_SKULL, RelicId::HELICAL_DART})) return 13;
    const auto silentRelics = data(RelicPoolId::SILENT_RELIC_POOL).relics;
    constexpr RelicId expectedSilentRelics[] = {
        RelicId::HELICAL_DART, RelicId::NINJA_SCROLL, RelicId::PAPER_KRANE, RelicId::RING_OF_THE_SNAKE,
        RelicId::SNECKO_SKULL, RelicId::TINGSHA, RelicId::TOUGH_BANDAGES, RelicId::TWISTED_FUNNEL};
    if (silentRelics.size() != std::size(expectedSilentRelics)) return 22;
    for (RelicId id : expectedSilentRelics)
        if (std::find(silentRelics.begin(), silentRelics.end(), id) == silentRelics.end()) return 22;
    for (RelicId id : silentRelics)
        if (std::find(std::begin(expectedSilentRelics), std::end(expectedSilentRelics), id) == std::end(expectedSilentRelics)) return 22;
    const auto silentPotions = data(PotionPoolId::SILENT_POTION_POOL).potions;
    if (silentPotions.size() != 3 ||
        std::find(silentPotions.begin(), silentPotions.end(), PotionId::POISON_POTION) == silentPotions.end() ||
        std::find(silentPotions.begin(), silentPotions.end(), PotionId::GHOST_IN_A_JAR) == silentPotions.end() ||
        std::find(silentPotions.begin(), silentPotions.end(), PotionId::CUNNING_POTION) == silentPotions.end()) return 23;
    return 0;
}
