#include <iostream>
#include <stdexcept>

#include "env/env.h"
#include "env/run_queries.h"
#include "game/All.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static std::unique_ptr<Env> enter_architect(int32_t ironclad_wins = 0) {
    auto env = std::make_unique<Env>();
    new_run(env->state(), NewRunOptions{.seed = "ARCHITECT", .character = CharacterId::IRONCLAD,
                                       .progress_character_wins = ironclad_wins});
    env->state().run.started_with_neow = false;
    RunManager::EnterAct(&env->sim(), 2);
    RunManager::EnterNextAct(&env->sim());
    return env;
}

int main() {
    auto env = enter_architect();
    const RunData& run = env->state().run;
    require(env->decision().kind == DecisionKind::Event, "The Architect did not enter as an event decision");
    require(run.rooms.size() == 1 && run.rooms.back() == RoomType::Event && run.room_event == EventId::THE_ARCHITECT,
            "The Architect did not retain its original victory room");
    require(env->state().combat.exists && env->state().combat.precreated_for_event_layout &&
                env->state().combat.encounter == EncounterId::THE_ARCHITECT_EVENT_ENCOUNTER &&
                CombatState(&env->sim()).Enemies().size() == 1 &&
                CombatState(&env->sim()).Enemies()[0].Monster().MonsterId_() == MonsterId::ARCHITECT,
            "The Architect combat-layout encounter was not initialized");
    require(run.events[0].rng.counter == 1, "first-win dialogue did not consume its singleton NextItem draw");
    require(run.events[0].options.size() == 1 &&
                run.events[0].options[0].text_key == MakeLocKey("THE_ARCHITECT.dialogue.0").hash,
            "first-win Ironclad dialogue choice differs from source");

    env->Step(Action{.kind = Action::EventOption, .index = 0});
    require(env->decision().kind == DecisionKind::Event && env->state().run.events[0].options.size() == 1 &&
                env->state().run.events[0].options[0].text_key == MakeLocKey("PROCEED").hash,
            "last Architect line did not present PROCEED");
    require(env->state().run.events[0].rng.counter == 1, "dialogue advance consumed extra event RNG");

    env->Step(Action{.kind = Action::EventOption, .index = 0});
    require(env->decision().kind == DecisionKind::Ended && env->state().run.phase == RunPhase::Ended &&
                env->state().run.is_victory,
            "Architect PROCEED did not record a terminal victory");
    require(env->state().run.rooms.size() == 1 && env->state().run.rooms.back() == RoomType::Event &&
                env->state().run.events[0].is_finished && env->state().run.events[0].options.empty(),
            "terminal victory did not retain the finished Architect room");
    require(Player(&env->sim(), 1).Creature().CurrentHp() == 0,
            "WinRun did not apply its post-run forced player death");
    require(ExportRunQueries(*env).find("\"is_victory\":true") != std::string::npos,
            "generic run queries did not expose the terminal victory result");
    const std::string victory_snapshot = env->ExportSnapshotJson();
    auto restored_victory = Env::ImportSnapshotJson(victory_snapshot);
    require(restored_victory->Hash() == env->Hash() &&
                restored_victory->state().run.is_victory &&
                restored_victory->ExportSnapshotJson() == victory_snapshot,
            "terminal Architect victory snapshot did not round-trip exactly");

    auto repeated = enter_architect(3);
    require(repeated->state().run.events[0].rng.counter == 0 &&
                repeated->state().run.events[0].options.size() == 1 &&
                repeated->state().run.events[0].options[0].text_key == MakeLocKey("PROCEED").hash,
            "an Ironclad without a matching TotalWins dialogue did not go directly to PROCEED");
    std::cout << "The Architect event passed\n";
}
