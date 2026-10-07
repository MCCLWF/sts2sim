// Test-only injected source, linked to the SAME public shared runtime as the
// consumer. No Handle casts, extra product exports or cross-runtime rebinding.
#include <algorithm>
#include <iostream>
#include "env/c_api.h"
#include "env/env.h"
#include "env/snapshot_json.h"
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"

using namespace sts2;
using snapshot_detail::Json;
template<class Names> static uint16_t named(const Names& names, const Json& value) {
    if (value.kind != Json::String) throw InvalidRequest("fixture model must be a string");
    for (size_t i = 1; i < names.size(); ++i) if (names[i] == value.text) return static_cast<uint16_t>(i);
    throw InvalidRequest("unknown fixture model " + value.text);
}
extern "C" int sts2_test_shop_fixture(void* diagnostics, const char* data, size_t length, void** result) {
    if (result) *result = nullptr;
    if (!diagnostics || !data || !result) return STS2_ENV_INVALID_REQUEST;
    try {
        const auto config = snapshot_detail::Parser(std::string_view(data, length)).parse();
        if (config.kind != Json::Object || config.object.size() != 5 || config.at("seed").kind != Json::String
            || config.at("setup_relics").kind != Json::Array) throw InvalidRequest("shop fixture configuration");
        const int gold = config.at("gold").integer<int>();
        if (gold < 0) throw InvalidRequest("negative fixture gold");
        Env source;
        new_run(source.state(), {.seed = config.at("seed").text,
            .character = static_cast<CharacterId>(named(kCharacterIdNames, config.at("character"))),
            .ascension = config.at("ascension").integer<int>()});
        // This is an explicit test injection, not a claim to support real
        // partial profiles or first-run epoch state.
        source.state().run.started_with_neow = false;
        source.Step({.kind = Action::StartRun});
        for (const auto& relic : config.at("setup_relics").array) {
            Operation obtain;
            obtain.kind = Operation::Kind::ObtainRelic;
            obtain.source_hash = source.Hash();
            obtain.relic_id = named(kRelicIdNames, relic);
            source.Execute(obtain);
            if (source.decision().kind != DecisionKind::Map) throw UnsupportedOperation("fixture setup requires choice-free relics");
        }
        PlayerCmd::SetGold(gold, Player(&source.sim(), 1));
        const auto legal = source.LegalActions();
        const auto move = std::find_if(legal.begin(), legal.end(), [](Action a) { return a.kind == Action::MapMove; });
        if (move == legal.end()) throw GameError("shop fixture has no map move");
        auto& map = source.state().run.map;
        const auto point = map.get_point(move->coord.col, move->coord.row);
        if (point < 0) throw GameError("shop fixture point is missing");
        map.points[static_cast<size_t>(point)].type = MapPointType::Shop;
        source.Step(*move);  // ordinary map/room entry, including all hooks
        if (source.decision().kind != DecisionKind::Shop) throw UnsupportedOperation("fixture room entry did not settle at Shop");
        const auto snapshot = source.ExportSnapshotJson();
        return sts2_env_import_snapshot(diagnostics, snapshot.data(), snapshot.size(), result);
    } catch (const std::exception& error) {
        std::cerr << "shop fixture: " << error.what() << '\n';
        return STS2_ENV_ERROR;
    } catch (...) {
        std::cerr << "shop fixture: unresolved effect or unexpected failure\n";
        return STS2_ENV_ERROR;
    }
}
