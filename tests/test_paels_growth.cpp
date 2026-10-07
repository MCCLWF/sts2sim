#include <stdexcept>

#include "engine/run_setup.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Entities/RestSite/RestSiteOption.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    GameState state;
    new_run(state, NewRunOptions{.seed = "PAELGROWTH", .character = CharacterId::IRONCLAD});
    Sim sim(state);
    Player player(&sim, 1);
    RunManager::SetActInternal(&sim, 0);
    {
        CardSelectCmd::ScopedDeterministicSelector selector(&sim);
        RelicCmd::Obtain(RelicId::PAELS_GROWTH, player);
    }
    CardModel selected;
    for (CardModel card : player.Deck().Cards()) {
        if (card.Enchantment() && card.Enchantment().EnchantmentId_() == EnchantmentId::CLONE) {
            selected = card;
            break;
        }
    }
    require(static_cast<bool>(selected) && selected.Enchantment().Amount() == 4,
            "Pael's Growth did not enchant the selected deck card");

    const auto options = RestSite::Generate(player);
    int32_t cloneOption = -1;
    for (std::size_t i = 0; i < options.size(); ++i)
        if (options[i] == RestSiteOptionId::Clone) cloneOption = static_cast<int32_t>(i);
    require(cloneOption >= 0 && RestSite::IsEnabled(player, RestSiteOptionId::Clone),
            "Pael's Growth did not add the Clone rest option");

    int deckBefore = static_cast<int>(player.Deck().Cards().size());
    RestSite::BeginRestSite(&sim);
    require(RestSite::ChooseOption(&sim, cloneOption), "Clone rest option failed");
    require(static_cast<int>(player.Deck().Cards().size()) == deckBefore + 1,
            "Clone rest option did not add a card");
    int copies = 0;
    for (CardModel card : player.Deck().Cards())
        if (card.CardId_() == selected.CardId_() && card.Enchantment() &&
            card.Enchantment().EnchantmentId_() == EnchantmentId::CLONE) ++copies;
    require(copies == 2, "cloned card lost its identity or enchantment");
}
