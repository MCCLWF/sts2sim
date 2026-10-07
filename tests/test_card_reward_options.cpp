#include "engine/run_setup.h"
#include "engine/sim/alloc.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/Relics/LastingCandy.h"
#include "game/Rewards/Reward.h"
#include "game/Runs/CardCreationOptions.h"

#include <cstdio>

using namespace sts2;

int main() {
    GameState state;
    Sim sim(state);
    new_run(state, NewRunOptions{.seed = "CANDYOPTIONS", .character = CharacterId::REGENT, .ascension = 10});
    Player player(&sim, 1);
    RelicModel rug(&sim, alloc_relic(state, RelicId::DINGY_RUG, 0));
    player.AddRelicInternal(rug, -1, true);
    CardCreationOptionsState options = CardCreationOptions::ForRoom(player, RoomType::Monster);
    options.flags = CardCreationFlags::IsFromCombat;
    Reward reward = Reward::NewCardReward(player, options, 3);
    reward.Populate();
    // Native RewardOptionsChecks observes both properties aliasing the
    // original record and retaining Colorless after DingyRug is removed.
    if (!reward.st().options.pools.contains(CardPoolId::COLORLESS_CARD_POOL) ||
        reward.st().options.pools != reward.st().reroll_options.pools) return 1;
    player.RemoveRelicInternal(rug, true);
    reward.Reroll();
    if (!reward.st().reroll_options.pools.contains(CardPoolId::COLORLESS_CARD_POOL) ||
        reward.st().options.pools != reward.st().reroll_options.pools) return 2;

    RelicModel candy(&sim, alloc_relic(state, RelicId::LASTING_CANDY, 0));
    player.AddRelicInternal(candy, -1, true);
    candy.st().extra[0] = 1;
    CardCreationOptionsState parent = CardCreationOptions::ForRoom(player, RoomType::Monster);
    parent.flags = CardCreationFlags::IsCardReward | CardCreationFlags::IsFromCombat | CardCreationFlags::NoUpgradeRoll;
    CardList cards;
    const int32_t before = player.PlayerRng().Rewards().counter;
    if (!candy.TryModifyCardRewardOptions(player, cards, parent)) return 3;
    // The shipped LastingCandy creates a new record without NoUpgradeRoll:
    // rarity, selection, and upgrade each consume one Rewards draw.
    if (cards.size() != 1 || cards[0].Type() != CardType::Power ||
        player.PlayerRng().Rewards().counter - before != 3 ||
        !has_flag(parent.flags, CardCreationFlags::NoUpgradeRoll)) return 4;
    std::puts("native reward options alias/reroll and LastingCandy flags passed");
    return 0;
}
