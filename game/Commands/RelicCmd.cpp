// Commands/RelicCmd.cs
#include "game/Commands/RelicCmd.h"

#include "engine/sim/alloc.h"
#include "game/Runs/RelicGrabBag.h"

namespace sts2::RelicCmd {

RelicModel Obtain(RelicModel relic, Player player, int32_t index) {
    RunState runState = player.RunState();
    // RelicChoices (run history) is not modelled.
    player.AddRelicInternal(relic, index);
    if (!relic.IsStackable()) {
        RelicGrabBagOps::Remove(player.st().relic_grab_bag, relic.RelicId_());
        RelicGrabBagOps::Remove(runState.st().shared_relic_grab_bag, relic.RelicId_());
    }
    relic.st().floor_added = static_cast<int32_t>(runState.TotalFloor());
    relic.AfterObtained();
    return relic;
}

RelicModel Obtain(RelicId relic, Player player, int32_t index) {
    return Obtain(RelicModel(&player.sim(), alloc_relic(*player.sim().g, relic, 0)), player, index);
}

void Remove(RelicModel relic) {
    Player player = relic.Owner();
    if (!player) throw GameError("Cannot remove an unowned relic");
    player.RemoveRelicInternal(relic);
    relic.AfterRemoved();
}

RelicModel Replace(RelicModel oldRelic, RelicModel replacement) {
    Player player = oldRelic.Owner();
    if (!player) throw GameError("Cannot replace an unowned relic");
    int32_t index = -1;
    for (size_t i = 0; i < player.st().relics.size(); ++i) {
        if (player.st().relics[i] == oldRelic.Index()) {
            index = static_cast<int32_t>(i);
            break;
        }
    }
    if (index < 0) throw GameError("Player does not have the relic");
    Remove(oldRelic);
    return Obtain(replacement, player, index);
}

RelicModel Replace(RelicModel oldRelic, RelicId replacement) {
    Player player = oldRelic.Owner();
    if (!player) throw GameError("Cannot replace an unowned relic");
    return Replace(oldRelic, RelicModel(&player.sim(), alloc_relic(*player.sim().g, replacement, 0)));
}

}  // namespace sts2::RelicCmd
