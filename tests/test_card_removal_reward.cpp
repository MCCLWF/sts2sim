#include <stdexcept>

#include "env/env.h"
#include "engine/run_setup.h"
#include "engine/sim/alloc.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Rewards/Reward.h"

using namespace sts2;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    GameState state;
    new_run(state, NewRunOptions{.seed = "REMOVALREWARD", .character = CharacterId::IRONCLAD});
    Sim sim(state);
    Player player(&sim, 1);
    const int32_t before = static_cast<int32_t>(player.Deck().Cards().size());
    Reward reward = Reward::NewCardRemovalReward(player);
    require(reward.RewardsSetIndex() == 7 && reward.IsPopulated(), "card removal reward metadata");
    reward.Populate();
    require(reward.IsPopulated(), "card removal reward population");
    {
        CardSelectCmd::ScopedDeterministicSelector selector(&sim);
        require(reward.SelectUnsynchronized(), "card removal reward selection failed");
    }
    require(static_cast<int32_t>(player.Deck().Cards().size()) == before - 1,
            "card removal reward did not remove one deck card");
    require(reward.SuccessfullySelected(), "card removal reward was not marked selected");

}
