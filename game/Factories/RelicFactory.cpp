// Factories/RelicFactory.cs
#include "game/Factories/RelicFactory.h"

namespace sts2::RelicFactory {

RelicId PullNextRelicFromFront(Player player, Rng& rng) { return PullNextRelicFromFront(player, RollRarity(rng)); }
RelicId PullNextRelicFromFront(Player player) { return PullNextRelicFromFront(player, RollRarity(player)); }

RelicId PullNextRelicFromFront(Player player, RelicRarity rarity, const RelicGrabBagOps::RelicFilter& filter) {
    Sim* sim = &player.sim();
    RelicId relicModel = RelicGrabBagOps::PullFromFront(sim, player.st().relic_grab_bag, rarity, filter);
    if (relicModel == RelicId::NONE) relicModel = FallbackRelic;
    RelicGrabBagOps::Remove(sim->g->run.shared_relic_grab_bag, relicModel);
    return relicModel;
}

RelicId PullNextRelicFromBack(Player player) { return PullNextRelicFromBack(player, RollRarity(player)); }

RelicId PullNextRelicFromBack(Player player, RelicRarity rarity, const RelicGrabBagOps::RelicFilter& filter) {
    Sim* sim = &player.sim();
    RelicId relicModel = RelicGrabBagOps::PullFromBack(sim, player.st().relic_grab_bag, rarity, filter);
    if (relicModel == RelicId::NONE) relicModel = FallbackRelic;
    RelicGrabBagOps::Remove(sim->g->run.shared_relic_grab_bag, relicModel);
    return relicModel;
}

RelicRarity RollRarity(Player player) { return RollRarity(player.PlayerRng().Rewards()); }

RelicRarity RollRarity(Rng& rng) {
    float num = rng.NextFloat();
    return num < 0.5f ? RelicRarity::Common : (!(num < 0.83f) ? RelicRarity::Rare : RelicRarity::Uncommon);
}

}  // namespace sts2::RelicFactory
