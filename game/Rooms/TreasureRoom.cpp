// See TreasureRoom.h.
#include "game/Rooms/TreasureRoom.h"

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Hooks/Hook.h"
#include "game/Runs/RelicGrabBag.h"

namespace sts2::TreasureRoom {

namespace {

void EndRelicVoting(Sim* sim) {
    TreasureState& t = sim->g->run.treasure;
    t.picking = false;
    t.current_relics.clear();
    t.singleplayer_skipped = false;
}

int32_t TryHandleSpoilsMap(Player player) {
    const RunState run = player.RunState();
    const int col = run.CurrentMapCol(), row = run.CurrentMapRow();
    if (col < 0 || row < 0) return 0;
    const int point = run.st().map.get_point(col, row);
    if (point < 0 || !run.st().map.points[static_cast<size_t>(point)].spoils_map_quest) return 0;

    CardList quests;
    for (CardModel card : player.Deck().Cards()) {
        if (card.CardId_() == CardId::SPOILS_MAP) quests.push_back(card);
    }
    int32_t gold = 0;
    for (CardModel card : quests) {
        PlayerCmd::GainGold(Dec(600), player);
        gold += 600;
        CardPileCmd::RemoveFromDeck(card, /*showPreview=*/false);
    }
    return gold;
}

}  // namespace

void BeginRelicPicking(Sim* sim) {
    TreasureState& t = sim->g->run.treasure;
    if (t.picking) throw GameError("Attempted to start new relic picking session while one was already occurring!");
    t = TreasureState{};
    t.picking = true;
    Player player(sim, 1);
    RunState runState = player.RunState();
    if (Hook::ShouldGenerateTreasure(runState, player)) {
        RelicRarity rarity = RelicFactory::RollRarity(sim->g->run.rng.TreasureRoomRelics());
        // TryGetRelicForTutorial needs NumberOfRuns == 0 (never, on the fully unlocked profile).
        RelicId item = RelicGrabBagOps::PullFromFront(sim, sim->g->run.shared_relic_grab_bag, rarity, nullptr);
        if (item == RelicId::NONE) item = RelicFactory::FallbackRelic;
        sim->g->run.treasure.current_relics.push_back(item);
    }
    if (sim->g->run.treasure.current_relics.empty()) EndRelicVoting(sim);
    // Single player: no other player votes.
}

int32_t DoNormalRewards(Sim* sim) {
    Player player(sim, 1);
    if (!Hook::ShouldGenerateTreasure(player.RunState(), player)) return 0;
    double gold = player.PlayerRng().Rewards().NextInt(42, 53);
    if (AscensionHelper::HasAscension(*sim, AscensionLevel::Poverty)) gold *= 0.75;  // PovertyAscensionGoldMultiplier
    PlayerCmd::GainGold(Dec(static_cast<int32_t>(gold)), player);
    double num = gold;
    gold = num + static_cast<double>(TryHandleSpoilsMap(player));
    return static_cast<int32_t>(gold);
}

void DoExtraRewardsIfNeeded(Sim* sim) {
    // RewardsCmd.GenerateForRoomEnd(player, this), then Offer.
    RewardsSet set = RewardsCmd::NewCustomSet(sim);
    set = RewardsSet::NewForRoom(sim, RoomType::Treasure);
    set.GenerateWithoutOffering();
    RewardsCmd::OfferAndAwait(set);
}

void PickRelic(Sim* sim, int32_t index) {
    TreasureState& t = sim->g->run.treasure;
    // PickRelicLocally: nothing while no picking is going on.
    if (!t.picking) return;
    // OnPicked
    if (index >= static_cast<int32_t>(t.current_relics.size())) throw GameError("Attempted to pick a relic index out of range");
    if (index < 0) {
        t.singleplayer_skipped = true;  // single player: the relics stay until the room is left
        return;
    }
    // AwardRelics: the only vote gives OnlyOnePlayerVoted; no relic is left
    // unvoted, so the StableShuffle of the leftovers draws nothing.
    RelicId relic = t.current_relics[static_cast<size_t>(index)];
    EndRelicVoting(sim);
    // AnimateRelicAwards: RelicCmd.Obtain(relic.ToMutable(), player); no other
    // player to MoveToFallback for.
    RelicCmd::Obtain(relic, Player(sim, 1));
}

void OnRoomExited(Sim* sim) {
    if (sim->g->run.treasure.singleplayer_skipped) EndRelicVoting(sim);
}

}  // namespace sts2::TreasureRoom
