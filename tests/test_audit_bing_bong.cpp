#include <cstddef>
#include <stdexcept>

#include "engine/run_setup.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Entities/RestSite/RestSiteOption.h"
#include "game/Models/Relics/BingBong.h"
#include "game/Runs/RunState.h"

using namespace sts2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_hatching_egg_triggers_bing_bong_and_lucky_fysh() {
    GameState state;
    Sim sim(state);
    new_run(state, NewRunOptions{.seed = "AUDITBINGBONG", .character = CharacterId::IRONCLAD});
    Player player(&sim, 1);

    // Add the Egg before these relics so the fixture starts with exactly one
    // Egg and neither relic observes its initial addition to the Deck.
    CardModel egg = player.RunState().CreateCard(CardId::BYRDONIS_EGG, player);
    CardPileCmd::Add(egg, PileType::Deck);
    int32_t initialEggCount = 0;
    int32_t initialSwoopCount = 0;
    for (CardModel card : player.Deck().Cards()) {
        initialEggCount += card.CardId_() == CardId::BYRDONIS_EGG;
        initialSwoopCount += card.CardId_() == CardId::BYRD_SWOOP;
    }
    require(initialEggCount == 1 && initialSwoopCount == 0,
            "fixture did not start with exactly one Egg and no Byrd Swoops");
    RelicModel bingBongRelic = RelicCmd::Obtain(RelicId::BING_BONG, player);
    RelicModel unrelatedCloneSource = RelicCmd::Obtain(RelicId::LUCKY_FYSH, player);
    const size_t deckSizeBeforeUnrelatedClone = player.Deck().Cards().size();
    BingBong(&sim, bingBongRelic.Index()).AfterCardChangedPiles(
        egg, PileType::Deck, unrelatedCloneSource);
    require(player.Deck().Cards().size() == deckSizeBeforeUnrelatedClone,
            "BingBong duplicated a card whose pile-change callback had another non-null clone source");
    const int32_t goldBeforeHatch = player.Gold();
    const size_t deckSizeBeforeHatch = player.Deck().Cards().size();
    require(deckSizeBeforeHatch > 0, "fixture Deck is unexpectedly empty");

    RestSite::BeginRestSite(&sim);
    int32_t hatchOption = -1;
    for (size_t i = 0; i < state.run.rest_site.options.size(); ++i) {
        if (state.run.rest_site.options[i] == RestSiteOptionId::Hatch)
            hatchOption = static_cast<int32_t>(i);
    }
    require(hatchOption >= 0, "Byrdonis Egg did not add HATCH to RestSite options");
    require(RestSite::ChooseOption(&sim, hatchOption), "HATCH option did not complete successfully");

    int32_t eggCount = 0;
    int32_t swoopCount = 0;
    for (CardModel card : player.Deck().Cards()) {
        eggCount += card.CardId_() == CardId::BYRDONIS_EGG;
        swoopCount += card.CardId_() == CardId::BYRD_SWOOP;
    }
    require(static_cast<bool>(player.GetRelic(RelicId::BYRDPIP)), "HATCH did not obtain Byrdpip");
    require(eggCount == 0, "HATCH left the original Byrdonis Egg in the Deck");
    require(swoopCount == 2, "BingBong did not duplicate the transformed Byrd Swoop exactly once");
    require(player.Deck().Cards().size() == deckSizeBeforeHatch + 1,
            "HATCH plus BingBong did not replace one Egg with two Swoops");
    require(player.Gold() == goldBeforeHatch + 30,
            "Lucky Fysh did not award 15 gold for each of the two Swoops entering the Deck");

    // CardsToSkip is an identity set, not just a recursion guard. A clone
    // returned to Deck (for example after Theft) consumes its one skip;
    // another later pile change may duplicate it again.
    CardModel returnedCopy;
    for (CardModel card : player.Deck().Cards())
        if (card.st().bing_bong_skip_relic == bingBongRelic.Index()) returnedCopy = card;
    require(static_cast<bool>(returnedCopy), "BingBong did not retain its generated clone identity");
    const int32_t deckBeforeReturn = player.Deck().Count();
    // CardPileCmd does not send pile-change hooks for Deck -> Deck. Theft
    // removes the actual deck object, so reproduce its return from no pile.
    player.Deck().RemoveInternal(returnedCopy);
    CardPileCmd::Add(returnedCopy, PileType::Deck);
    require(player.Deck().Count() == deckBeforeReturn,
            "returning BingBong's own clone should consume the skip without another copy");
    player.Deck().RemoveInternal(returnedCopy);
    CardPileCmd::Add(returnedCopy, PileType::Deck);
    require(player.Deck().Count() == deckBeforeReturn + 1,
            "BingBong's skip must be consumed exactly once");

    CardModel rememberedCopy;
    for (CardModel card : player.Deck().Cards())
        if (card.st().bing_bong_skip_relic == bingBongRelic.Index()) rememberedCopy = card;
    CardModel independentCopy = player.RunState().CloneCard(rememberedCopy);
    const int32_t deckBeforeIndependentClone = player.Deck().Count();
    CardPileCmd::Add(independentCopy, PileType::Deck);
    require(player.Deck().Count() == deckBeforeIndependentClone + 2,
            "a new mutable clone must not inherit membership in the original identity set");
}

}  // namespace

int main() {
    test_hatching_egg_triggers_bing_bong_and_lucky_fysh();
}
