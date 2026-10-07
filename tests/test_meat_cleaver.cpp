#include <stdexcept>
#include <algorithm>

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
    new_run(state, NewRunOptions{.seed = "MEATCLEAVER", .character = CharacterId::IRONCLAD});
    Sim sim(state);
    Player player(&sim, 1);
    RunManager::SetActInternal(&sim, 0);
    RelicModel relic = RelicCmd::Obtain(RelicId::MEAT_CLEAVER, player);
    require(relic && IsUsable(ModelKind::Relic, relic.DenseId()), "Meat Cleaver remained unusable");

    StaticVec<RestSiteOptionId, 8> options = RestSite::Generate(player);
    int cook_index = -1;
    for (size_t i = 0; i < options.size(); ++i)
        if (options[i] == RestSiteOptionId::Cook) cook_index = static_cast<int>(i);
    require(cook_index >= 0, "Meat Cleaver did not add Cook");
    require(RestSite::IsEnabled(player, RestSiteOptionId::Cook), "Cook disabled with removable cards available");

    int32_t max_hp_before = player.Creature().MaxHp();
    int32_t current_hp_before = player.Creature().CurrentHp();
    int32_t removable_before = 0;
    for (CardModel card : player.Deck().Cards()) removable_before += card.IsRemovable();
    require(removable_before >= 2, "Cook test deck lacks removable cards");
    RestSite::BeginRestSite(&sim);
    {
        CardSelectCmd::ScopedDeterministicSelector selector(&sim);
        require(RestSite::ChooseOption(&sim, cook_index), "Cook selection was rejected");
    }
    int32_t removable_after = 0;
    for (CardModel card : player.Deck().Cards()) removable_after += card.IsRemovable();
    require(removable_after == removable_before - 2, "Cook did not remove exactly two removable cards");
    require(player.Creature().MaxHp() == max_hp_before + 5, "Cook did not add five max HP");
    require(player.Creature().CurrentHp() == std::min(player.Creature().MaxHp(), current_hp_before + 5),
            "Cook max HP gain did not heal by the gained amount");

    // The option remains listed when disabled, but cannot be used with fewer
    // than two removable cards, matching CookRestSiteOption.IsEnabled.
    for (CardModel card : player.Deck().Cards()) card.AddKeyword(CardKeyword::Eternal);
    options = RestSite::Generate(player);
    require(options.contains(RestSiteOptionId::Cook), "Cook disappeared instead of becoming disabled");
    require(!RestSite::IsEnabled(player, RestSiteOptionId::Cook), "Cook stayed enabled with no removable cards");
}
