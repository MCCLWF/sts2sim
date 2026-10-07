#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "env/preview.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Runs/RunManager.h"

using namespace sts2;
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

// Contract fixture: ordinary Boss room entry and lethal card action, with a
// shortened enemy HP. Gameplay fidelity is tested independently by L4 oracle.
static std::unique_ptr<Env> boss_rewards(int act_index, int ascension = 0) {
    auto env = std::make_unique<Env>();
    new_run(env->state(), NewRunOptions{.seed = "ACTPROCEED", .character = CharacterId::IRONCLAD,
                                      .ascension = ascension});
    auto& run = env->state().run;
    run.started_with_neow = false;
    run.acts[static_cast<size_t>(act_index)].rooms.boss = EncounterId::CEREMONIAL_BEAST_BOSS;
    RunManager::EnterAct(&env->sim(), act_index);
    const auto& map = run.map;
    const auto point = std::find_if(map.points.begin(), map.points.end(), [&](const auto& p) {
        return p.coord.row == map.rows - 1;
    });
    require(point != map.points.end(), "fixture missing pre-Boss map point");
    run.visited_map_coords.push_back(point->coord);
    env->Step(Action{.kind = Action::MapMove, .coord = map.points[map.boss].coord});
    const auto enemies = CombatState(&env->sim()).Enemies();
    require(enemies.size() == 1, "fixture Boss roster");
    enemies[0].st().current_hp = 1;
    enemies[0].st().block = 0;
    Player player(&env->sim(), 1);
    CardModel strike = CombatState(&env->sim()).CreateCard(CardId::STRIKE_IRONCLAD, player);
    CardPileCmd::Add(strike, PileType::Hand);
    env->Step(Action{.kind = Action::PlayCard, .index = player.PlayerCombatState().Hand().Count() - 1,
                     .target = enemies[0].CombatId()});
    require(env->decision().kind == DecisionKind::Rewards, "Boss did not offer its terminal reward screen");
    return env;
}

int main() {
    const Action proceed{.kind = Action::Proceed};
    for (int index : {0, 1}) {
        auto source = boss_rewards(index);
        const auto before = source->Hash();
        const auto total_floor = source->state().run.total_floor;
        auto restored_source = Env::ImportSnapshotJson(source->ExportSnapshotJson());
        auto result = PreviewAction(*source, proceed, {.capture_run_results = true});
        auto& next = *result.successor;
        require(source->Hash() == before, "Boss preview mutated source");
        require(next.Hash() == source->Predict(proceed)->Hash()
            && next.Hash() == restored_source->Predict(proceed)->Hash(), "Boss restore/preview successor mismatch");
        require(next.decision().kind == DecisionKind::Map && next.state().run.current_act_index == index + 1,
            "Boss Proceed failed to enter the next act map");
        require(next.state().run.act_floor == 0 && next.state().run.total_floor == total_floor,
            "act transition confused act floor and visited floor count");
        require(next.state().run.visited_map_coords.empty() && next.state().run.rooms.size() == 1
            && next.state().run.rooms.back() == RoomType::Map && !next.state().combat.exists,
            "act transition retained old room/combat");
        require(!next.LegalActions().empty(), "new act map has no legal start move");
        auto restored = Env::ImportSnapshotJson(next.ExportSnapshotJson());
        require(restored->Hash() == next.Hash() && restored->LegalActions() == next.LegalActions(),
            "new act map snapshot failed to restore");
        require(!result.ExportJson().empty(), "new act result queries failed");
    }

    auto first = boss_rewards(2, 10);
    require(first->state().run.map.second_boss >= 0, "double Boss fixture missing second Boss");
    const auto floor = first->state().run.act_floor;
    auto second_map = first->Predict(proceed);
    require(second_map->decision().kind == DecisionKind::Map && second_map->state().run.current_act_index == 2
        && second_map->state().run.act_floor == floor, "first of double Bosses incorrectly advanced act");
    const auto actions = second_map->LegalActions();
    require(actions.size() == 1 && actions[0].coord == second_map->state().run.map.points[
        static_cast<size_t>(second_map->state().run.map.second_boss)].coord, "second Boss map option missing");
    auto restored = Env::ImportSnapshotJson(second_map->ExportSnapshotJson());
    require(restored->Hash() == second_map->Hash(), "double Boss map restore failed");

    // Final Boss Proceed enters the real Architect event room; its dialogue
    // and WinRun are covered by the focused event test and L4 oracle route.
    auto final = boss_rewards(2);
    const auto final_hash = final->Hash();
    auto victory_room = final->Predict(proceed);
    require(final->Hash() == final_hash, "final-room prediction mutated source");
    require(victory_room->decision().kind == DecisionKind::Event &&
                victory_room->state().run.room_event == EventId::THE_ARCHITECT &&
                victory_room->state().run.rooms.back() == RoomType::Event &&
                victory_room->state().combat.precreated_for_event_layout &&
                !victory_room->state().run.is_victory,
            "final Boss Proceed failed to enter the Architect event room");
    auto restored_victory_room = Env::ImportSnapshotJson(victory_room->ExportSnapshotJson());
    require(restored_victory_room->Hash() == victory_room->Hash() &&
                restored_victory_room->LegalActions() == victory_room->LegalActions(),
            "Architect room snapshot failed to restore");
    std::cout << "act transition contracts passed\n";
}
