// Differential-only injection of the same Act 3 Trial root as the oracle.
// All subsequent operations go through the public Env ABI.
#include <iostream>
#include "env/c_api.h"
#include "env/env.h"
#include "env/snapshot_json.h"
#include "engine/sim/alloc.h"
#include "game/All.h"
#include "game/Commands/RelicCmd.h"
#include "game/Hooks/Hook.h"

using namespace sts2;
extern "C" int sts2_test_trial_fixture(void* diagnostics, const char* data, size_t length, void** result) {
    if (result) *result = nullptr;
    if (!diagnostics || !data || !result) return STS2_ENV_INVALID_REQUEST;
    try {
        const auto config = snapshot_detail::Parser(std::string_view(data, length)).parse();
        Env source;
        source.Reset({.seed = config.at("seed").text, .character = CharacterId::SILENT, .ascension = 10});
        Player player(&source.sim(), 1);
        if (config.at("prevent_death").boolean)
            RelicCmd::Obtain(RelicModel(&source.sim(), alloc_relic(source.state(), RelicId::LIZARD_TAIL, 0)), player);
        EventModel(&source.sim()).EnsureCleanup();
        source.state().run.current_act_index = 2;
        source.state().run.act_floor = 0;
        source.state().run.visited_map_coords.clear();
        source.state().run.rooms.clear();
        source.state().run.rooms.push_back(RoomType::Event);
        source.state().run.room_event = EventId::TRIAL;
        Hook::BeforeRoomEntered(RunState(&source.sim()), AbstractRoom(&source.sim(), RoomType::Event));
        auto event = EventModel::ToMutable(&source.sim(), EventId::TRIAL);
        event.BeginEvent(player, false);
        Hook::AfterRoomEntered(RunState(&source.sim()), AbstractRoom(&source.sim(), RoomType::Event));
        event.AfterEventStarted();
        source.state().run.phase = RunPhase::Event;
        const auto snapshot = source.ExportSnapshotJson();
        return sts2_env_import_snapshot(diagnostics, snapshot.data(), snapshot.size(), result);
    } catch (const std::exception& error) {
        std::cerr << "Trial fixture: " << error.what() << '\n';
        return STS2_ENV_ERROR;
    }
}
