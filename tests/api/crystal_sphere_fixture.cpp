// Test-only source injection. Operations after this root use the public C ABI.
#include <iostream>
#include "env/c_api.h"
#include "env/env.h"
#include "env/snapshot_json.h"
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Hooks/Hook.h"

using namespace sts2;
extern "C" int sts2_test_crystal_sphere_fixture(void* diagnostics, const char* data, size_t length, void** result) {
    if (result) *result = nullptr;
    if (!diagnostics || !data || !result) return STS2_ENV_INVALID_REQUEST;
    try {
        const auto config = snapshot_detail::Parser(std::string_view(data, length)).parse();
        if (config.kind != snapshot_detail::Json::Object || config.object.size() != 4)
            throw InvalidRequest("Crystal Sphere fixture configuration");
        CharacterId character = CharacterId::NONE;
        for (size_t i = 1; i < kCharacterIdNames.size(); ++i)
            if (kCharacterIdNames[i] == config.at("character").text) character = static_cast<CharacterId>(i);
        if (character == CharacterId::NONE || config.at("gold").integer<int>() < 0)
            throw InvalidRequest("Crystal Sphere fixture character/gold");
        Env source;
        source.Reset({.seed = config.at("seed").text, .character = character,
                      .ascension = config.at("ascension").integer<int>()});
        Player player(&source.sim(), 1);
        PlayerCmd::SetGold(config.at("gold").integer<int>(), player);
        // Matches the explicit oracle room injection, not a natural Act 2 path.
        // Source EventRoom entry calls BeginEvent, room hooks, AfterEventStarted.
        EventModel(&source.sim()).EnsureCleanup();
        source.state().run.current_act_index = 1;
        source.state().run.act_floor = 0; // RunState.CurrentActIndex setter
        source.state().run.visited_map_coords.clear();
        source.state().run.rooms.clear();
        source.state().run.rooms.push_back(RoomType::Event);
        source.state().run.room_event = EventId::CRYSTAL_SPHERE;
        Hook::BeforeRoomEntered(RunState(&source.sim()), AbstractRoom(&source.sim(), RoomType::Event));
        auto event = EventModel::ToMutable(&source.sim(), EventId::CRYSTAL_SPHERE);
        event.BeginEvent(player, false);
        Hook::AfterRoomEntered(RunState(&source.sim()), AbstractRoom(&source.sim(), RoomType::Event));
        event.AfterEventStarted();
        source.state().run.phase = RunPhase::Event;
        const auto snapshot = source.ExportSnapshotJson();
        return sts2_env_import_snapshot(diagnostics, snapshot.data(), snapshot.size(), result);
    } catch (const std::exception& error) {
        std::cerr << "Crystal Sphere fixture: " << error.what() << '\n';
        return STS2_ENV_ERROR;
    }
}
