#include "env/c_api.h"
#include "env/combat_prediction.h"
#include "env/combat_root_inspect.h"
#include "env/combat_prediction_queries.h"
#include "engine/json.h"

#include <memory>
#include <string>

namespace {
struct PredictionHandle {
    explicit PredictionHandle(std::unique_ptr<sts2::CombatPredictionSession> value = {})
        : session(std::move(value)) {}
    std::unique_ptr<sts2::CombatPredictionSession> session;
    std::string error;
    std::string result;
};

template<class F> int prediction_call(PredictionHandle& handle, F&& operation) {
    handle.error.clear();
    handle.result.clear();
    try {
        operation();
        return STS2_ENV_OK;
    } catch (const sts2::Unported& error) {
        handle.error = error.what();
        return STS2_ENV_UNPORTED;
    } catch (const std::exception& error) {
        handle.error = error.what();
        return STS2_ENV_ERROR;
    }
}

const sts2::CombatPredictionSession& session(PredictionHandle& handle) {
    if (!handle.session) throw sts2::GameError("combat prediction: diagnostic handle has no session");
    return *handle.session;
}

int import_prediction(void* raw, const char* data, size_t length, void** successor, bool game_snapshot) {
    if (successor) *successor = nullptr;
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    // data may be a borrowed pointer into this handle's result. Copy before
    // resetting either diagnostics buffer, including on parse failure.
    if (!successor || !data || length == 0 || length > 4 * 1024 * 1024) {
        return prediction_call(handle, [] { throw sts2::GameError("combat prediction: invalid snapshot input"); });
    }
    std::string owned;
    try { owned.assign(data, length); }
    catch (const std::exception& error) { handle.error = error.what(); return STS2_ENV_ERROR; }
    return prediction_call(handle, [&] {
        auto restored = game_snapshot
            ? sts2::CombatPredictionSession::FromGameSnapshot(owned)
            : sts2::CombatPredictionSession::ImportSnapshotJson(owned);
        *successor = new PredictionHandle(std::move(restored));
    });
}
} // namespace

extern "C" void* sts2_combat_prediction_create() {
    try { return new PredictionHandle(); } catch (...) { return nullptr; }
}

extern "C" void sts2_combat_prediction_destroy(void* raw) {
    delete static_cast<PredictionHandle*>(raw);
}

extern "C" int sts2_combat_prediction_from_game_snapshot(void* raw, const char* data, size_t length, void** successor) {
    return import_prediction(raw, data, length, successor, true);
}

extern "C" int sts2_combat_prediction_import_snapshot(void* raw, const char* data, size_t length, void** successor) {
    return import_prediction(raw, data, length, successor, false);
}

extern "C" int sts2_combat_prediction_clone(void* raw, void** successor) {
    if (successor) *successor = nullptr;
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] {
        if (!successor) throw sts2::GameError("combat prediction: successor pointer is null");
        *successor = new PredictionHandle(session(handle).Clone());
    });
}

extern "C" int sts2_combat_prediction_predict(void* raw, sts2_action selected, void** successor) {
    if (successor) *successor = nullptr;
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] {
        if (!successor) throw sts2::GameError("combat prediction: successor pointer is null");
        if ((selected.kind != sts2::Action::PlayCard && selected.kind != sts2::Action::EndTurn
             && selected.kind != sts2::Action::UsePotion && selected.kind != sts2::Action::Answer)
            || selected.col != 0 || selected.row != 0)
            throw sts2::GameError("combat prediction: invalid combat action kind or coordinates");
        sts2::Action action{.kind = static_cast<sts2::Action::Kind>(selected.kind),
            .index = selected.index, .target = selected.target};
        *successor = new PredictionHandle(session(handle).Predict(action));
    });
}

extern "C" int sts2_combat_prediction_export_snapshot(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] { handle.result = session(handle).ExportSnapshotJson(); });
}

extern "C" int sts2_combat_prediction_inspect_current(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] {
        const auto& branch = session(handle);
        const uint64_t before = branch.Hash();
        auto scratch = branch.environment().Clone();
        sts2::JsonWriter writer;
        writer.begin_object().field("prediction_hash", before)
            .field("state_hash", scratch->Hash())
            .field("decision", static_cast<int32_t>(scratch->decision().kind))
            .field("terminal", branch.terminal().has_value());
        sts2::write_combat_root(writer, *scratch);
        writer.end_object();
        if (branch.Hash() != before)
            throw sts2::GameError("combat prediction inspect changed source");
        handle.result = writer.str();
    });
}

extern "C" int sts2_combat_prediction_export_queries(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] { handle.result = sts2::ExportCombatPredictionQueries(session(handle)); });
}

extern "C" int sts2_combat_prediction_export_calculated_vars(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] { handle.result = sts2::ExportCombatPredictionCalculatedVars(session(handle)); });
}

extern "C" int sts2_combat_prediction_export_orbs(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] { handle.result = sts2::ExportCombatPredictionOrbs(session(handle)); });
}

extern "C" int sts2_combat_prediction_legal_actions(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] {
        const auto& branch = session(handle);
        sts2::JsonWriter writer;
        writer.begin_object().field("format", "sts2env.combat_prediction_actions").field("version", 1)
            .field("prediction_hash", branch.Hash()).key("actions").begin_array();
        for (const auto& action : branch.LegalActions()) {
            writer.begin_object().field("kind", static_cast<int32_t>(action.kind))
                .field("index", action.index).field("target", action.target).end_object();
        }
        writer.end_array().end_object();
        handle.result = writer.str();
    });
}

extern "C" int sts2_combat_prediction_export_power_queries(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] { handle.result = sts2::ExportCombatPredictionPowers(session(handle)); });
}

extern "C" int sts2_combat_prediction_hash(void* raw, uint64_t* value) {
    if (value) *value = 0;
    if (!raw) return STS2_ENV_ERROR;
    auto& handle = *static_cast<PredictionHandle*>(raw);
    return prediction_call(handle, [&] {
        if (!value) throw sts2::GameError("combat prediction: hash pointer is null");
        *value = session(handle).Hash();
    });
}

extern "C" const char* sts2_combat_prediction_last_result(void* raw) {
    return raw ? static_cast<PredictionHandle*>(raw)->result.c_str() : "";
}

extern "C" const char* sts2_combat_prediction_last_error(void* raw) {
    return raw ? static_cast<PredictionHandle*>(raw)->error.c_str() : "combat prediction: null handle";
}
