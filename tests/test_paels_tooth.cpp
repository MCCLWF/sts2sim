#include <array>
#include <stdexcept>

#include "engine/sim/gc.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/Cards/Rampage.h"
#include "game/Models/Relics/PaelsTooth.h"
#include "game/Runs/RunState.h"

using namespace sts2;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    Env env;
    env.Reset(NewRunOptions{.seed = "PAELTOOTH01", .character = CharacterId::IRONCLAD});
    Player player(&env.sim(), 1);
    const int initialDeck = static_cast<int>(player.Deck().Cards().size());
    RelicModel relic;
    const std::array<int32_t, 6> selection = {5, 5, 6, 7, 8, 9};
    env.sim().set_prefix(selection);
    relic = RelicCmd::Obtain(RelicId::PAELS_TOOTH, player);
    PaelsTooth tooth(&env.sim(), relic.Index());
    require(tooth.DisplayAmount() == 5 && tooth.ShowCounter() &&
                static_cast<int>(player.Deck().Cards().size()) == initialDeck - 5,
            "Pael's Tooth did not store five selected cards");
    require(env.sim().prefix_consumed(), "Pael's Tooth did not consume the explicit card selection");
    require(env.state().relics[relic.Index()].paels_tooth_cards.size() == 5 &&
                env.state().cards[env.state().relics[relic.Index()].paels_tooth_cards[0]].id == CardId::BASH &&
                env.state().cards[env.state().relics[relic.Index()].paels_tooth_cards[1]].id == CardId::DEFEND_IRONCLAD,
            "Pael's Tooth did not sort the saved card selection by model entry");
    CollectGarbage(env.state());
    const std::string snapshot = env.ExportSnapshotJson();
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->ExportSnapshotJson() == snapshot,
            "Pael's Tooth saved cards failed snapshot restore after garbage collection");

    for (int remaining = 4; remaining >= 0; --remaining) {
        tooth.AfterCombatEnd(CombatRoom{});
        if (remaining == 4) {
            CardId returned = player.Deck().Cards().back().CardId_();
            require(returned == CardId::BASH,
                    "Pael's Tooth reward RNG did not select the oracle's returned card");
        }
        require(tooth.DisplayAmount() == remaining &&
                    static_cast<int>(player.Deck().Cards().size()) == initialDeck - remaining,
                "Pael's Tooth returned the wrong number of cards");
    }
    require(!tooth.ShowCounter() && tooth.Status() == RelicStatus::Disabled,
            "Pael's Tooth did not disable after returning all cards");
    int upgraded = 0;
    for (CardModel card : player.Deck().Cards()) upgraded += card.CurrentUpgradeLevel() > 0;
    require(upgraded == 5, "returned cards were not upgraded");

    Env transientEnv;
    transientEnv.Reset(NewRunOptions{.seed = "PAELTOOTHSERIAL", .character = CharacterId::IRONCLAD});
    Player transientPlayer(&transientEnv.sim(), 1);
    transientPlayer.Deck().refs().clear();
    for (int i = 0; i < 5; ++i) {
        CardModel rampage = transientPlayer.RunState().CreateCard(CardId::RAMPAGE, transientPlayer);
        rampage.SetGrowthDamage(Dec(17));  // Runtime growth is not in SerializableCard.
        CardCmd::Enchant(EnchantmentId::CLONE, rampage, Dec(4));
        CardPileCmd::Add(rampage, PileType::Deck);
    }
    RelicModel serialRelic;
    {
        CardSelectCmd::ScopedDeterministicSelector select(&transientEnv.sim());
        serialRelic = RelicCmd::Obtain(RelicId::PAELS_TOOTH, transientPlayer);
    }
    PaelsTooth serialTooth(&transientEnv.sim(), serialRelic.Index());
    serialTooth.AfterCombatEnd(CombatRoom{});
    CardList restoredRampages = transientPlayer.Deck().Cards();
    require(restoredRampages.size() == 1 && restoredRampages[0].CardId_() == CardId::RAMPAGE &&
                restoredRampages[0].GrowthDamage() == Dec(0) && restoredRampages[0].CurrentUpgradeLevel() == 1 &&
                restoredRampages[0].Enchantment() &&
                restoredRampages[0].Enchantment().EnchantmentId_() == EnchantmentId::CLONE &&
                restoredRampages[0].Enchantment().Amount() == 4,
            "Pael's Tooth did not restore only serializable card state");
}
