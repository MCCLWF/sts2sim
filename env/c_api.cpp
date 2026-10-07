#include "env/c_api.h"

#include <memory>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "content/generated/ids.h"
#include "engine/json.h"
#include "engine/sim/errors.h"
#include "env/env.h"
#include "env/combat_root_inspect.h"
#include "env/snapshot_internal.h"
#include "game/All.h"
#include "env/preview.h"
#include "env/game_queries.h"
#include "env/run_queries.h"
#include "env/c_action_validation.h"
#include "env/resource_limits.h"
#include "tools/cli/dump.h"

namespace {
struct Handle {
    explicit Handle(std::unique_ptr<sts2::Env> state = std::make_unique<sts2::Env>())
        : env(std::move(state)) {}
    std::unique_ptr<sts2::Env> env;
    std::string error;
    std::string result;
};

template <class F>
int call(Handle& h, F&& f) {
    try {
        h.error.clear();
        h.result.clear();
        f();
        return STS2_ENV_OK;
    } catch (const sts2::TraceLimitExceeded& e) {
        h.error = e.what();
        return STS2_ENV_TRACE_LIMIT;
    } catch (const sts2::Unported& e) {
        h.error = e.what();
        return STS2_ENV_UNPORTED;
    } catch (const sts2::CapacityError& e) {
        h.error = e.what(); return STS2_ENV_CAPACITY;
    } catch (const sts2::UnsupportedOperation& e) {
        h.error = e.what(); return STS2_ENV_UNSUPPORTED;
    } catch (const sts2::InvalidRequest& e) {
        h.error = e.what(); return STS2_ENV_INVALID_REQUEST;
    } catch (const std::bad_alloc&) {
        h.error = "native host memory resource exhausted"; return STS2_ENV_CAPACITY;
    } catch (const std::exception& e) {
        h.error = e.what();
        return STS2_ENV_ERROR;
    } catch (...) {
        h.error = "unexpected native execution failure"; return STS2_ENV_ERROR;
    }
}

sts2::Action action_from_c(sts2_action a) {
    return sts2::action_from_c_checked(a);
}

sts2::CharacterId character_from_c(int32_t value) {
    using sts2::CharacterId;
    for (CharacterId character : {CharacterId::IRONCLAD, CharacterId::SILENT, CharacterId::REGENT,
                                  CharacterId::NECROBINDER, CharacterId::DEFECT})
        if (value == static_cast<int32_t>(character)) return character;
    throw sts2::InvalidRequest("reset: invalid single-player character ID");
}
sts2::EncounterId encounter_from_c(int32_t value) {
    if (value <= 0 || static_cast<size_t>(value) >= sts2::kEncounterIdNames.size())
        throw sts2::InvalidRequest("reset_combat: encounter ID out of range");
    return static_cast<sts2::EncounterId>(value);
}

sts2::RunQueryOptions query_options(int32_t flags) {
    if (flags != 0 && flags != 1) throw sts2::InvalidRequest("run queries: flags must be 0 or 1");
    return {.include_future = flags == 1};
}

sts2::PreviewOptions preview_options(int32_t trace_limit,int32_t flags) {
    if(flags<0 || (flags & ~7)!=0) throw sts2::InvalidRequest("preview: unsupported flags");
    return {.collect_trace=trace_limit!=0,.trace_limit=static_cast<size_t>(trace_limit),
        .capture_run_results=true,.run_query_options={.include_future=(flags & 1)!=0},.collect_rng=(flags & 2)!=0,
        .collect_causal=(flags & 4)!=0};
}

sts2_action action_to_c(const sts2::Action& a) {
    return sts2_action{static_cast<int32_t>(a.kind), a.index, a.target, a.coord.col, a.coord.row};
}


template <class Names>
uint16_t find_named_id(const Names& names, const char* text, const char* kind) {
    for (size_t i = 1; i < names.size(); ++i) {
        if (names[i] == text) return static_cast<uint16_t>(i);
    }
    throw sts2::InvalidRequest(std::string("unknown ") + kind + " " + text);
}

void reset_combat_named(sts2::Env& env, const char* seed, const char* character, int32_t ascension,
                        const char* encounter, int32_t act_index) {
    env.ResetCombat(sts2::NewRunOptions{
                        .seed = seed,
                        .character = static_cast<sts2::CharacterId>(find_named_id(sts2::kCharacterIdNames, character, "character")),
                        .ascension = ascension},
                    static_cast<sts2::EncounterId>(find_named_id(sts2::kEncounterIdNames, encounter, "encounter")),
                    act_index);
}
}  // namespace

extern "C" void* sts2_env_create() { try { return new Handle(); } catch (...) { return nullptr; } }
extern "C" void* sts2_env_clone(void* raw) {
    if (!raw) return nullptr;
    try {
        auto* source = static_cast<Handle*>(raw);
        return new Handle(source->env->Clone());
    } catch (...) { return nullptr; }
}
extern "C" void sts2_env_destroy(void* handle) { delete static_cast<Handle*>(handle); }

extern "C" int sts2_env_reset(void* raw, const char* seed, int32_t character, int32_t ascension) {
    if (!raw || !seed) return -1;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] {
        h.env->Reset(sts2::NewRunOptions{.seed = seed, .character = character_from_c(character), .ascension = ascension});
    });
}

extern "C" int sts2_env_reset_named(void* raw, const char* seed, const char* character, int32_t ascension) {
    if (!raw || !seed || !character) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] {
        h.env->Reset(sts2::NewRunOptions{
            .seed = seed,
            .character = static_cast<sts2::CharacterId>(find_named_id(sts2::kCharacterIdNames, character, "character")),
            .ascension = ascension});
    });
}

extern "C" int sts2_env_reset_combat(void* raw, const char* seed, int32_t character, int32_t ascension,
                                      int32_t encounter, int32_t act_index) {
    if (!raw || !seed) return -1;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] {
        h.env->ResetCombat(sts2::NewRunOptions{.seed = seed, .character = character_from_c(character),
                                               .ascension = ascension},
                           encounter_from_c(encounter), act_index);
    });
}

extern "C" int sts2_env_inspect_combat_named(void* raw, const char* seed, const char* character,
                                               int32_t ascension, const char* encounter, int32_t act_index) {
    if (!raw || !seed || !character || !encounter) return -1;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] {
        reset_combat_named(*h.env, seed, character, ascension, encounter, act_index);
        sts2::JsonWriter writer;
        writer.begin_object().field("state_hash", h.env->Hash());
        sts2::write_combat_root(writer, *h.env);
        writer.end_object();
        h.result = writer.str();
    });
}

extern "C" int sts2_env_inspect_current(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] {
        sts2::DecisionKind kind = h.env->decision().kind;
        sts2::JsonWriter writer;
        writer.begin_object().field("state_hash", h.env->Hash())
            .field("decision", static_cast<int32_t>(kind));
        if (kind == sts2::DecisionKind::Combat) sts2::write_combat_root(writer, *h.env);
        writer.end_object();
        h.result = writer.str();
    });
}

extern "C" int sts2_env_export_oracle_combat_state(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] {
        if (h.env->decision().kind != sts2::DecisionKind::Combat)
            throw sts2::GameError("oracle combat dump requires a combat decision");
        sts2::JsonWriter writer;
        sts2::write_combat_state(writer, &h.env->sim());
        h.result = writer.str();
    });
}

extern "C" int sts2_env_export_oracle_run_state(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] {
        sts2::JsonWriter writer;
        sts2::write_run(writer, &h.env->sim());
        h.result = writer.str();
    });
}

extern "C" int sts2_env_export_oracle_rewards(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] {
        const auto decision = h.env->decision();
        if (decision.kind != sts2::DecisionKind::Rewards &&
            !(decision.kind == sts2::DecisionKind::Choice && decision.request.source == "rewards"))
            throw sts2::GameError("oracle rewards dump requires a rewards decision");
        sts2::JsonWriter writer;
        writer.begin_array();
        sts2::Sim* sim = &h.env->sim();
        for (sts2::RewardRef ref : sim->g->run.rewards.rewards) {
            sts2::Reward reward(sim, ref);
            writer.begin_object().key("reward");
            sts2::write_reward(writer, sim, reward);
            writer.field("selected", reward.SuccessfullySelected()).end_object();
        }
        writer.end_array();
        h.result = writer.str();
    });
}

extern "C" int32_t sts2_env_event_rng_counter(void* raw) {
    if (!raw) return -1;
    const auto& run = static_cast<Handle*>(raw)->env->sim().g->run;
    return run.events[0].id == sts2::EventId::NONE ? -1 : run.events[0].rng.counter;
}

extern "C" int sts2_env_validate_combat_root(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] { h.env->ValidateCombatRoot(); });
}

extern "C" int sts2_env_export_snapshot(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] { h.result = h.env->ExportSnapshotJson(); });
}

extern "C" int sts2_env_export_portable_snapshot(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h=*static_cast<Handle*>(raw);
    return call(h,[&] { h.result=h.env->ExportPortableSnapshotJson(); });
}

extern "C" int sts2_env_import_snapshot(void* raw, const char* data, size_t length, void** successor) {
    if (successor) *successor = nullptr;
    if (!raw || !successor) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    if (!data)
        return call(h, [] { throw sts2::GameError("snapshot: invalid input buffer or length"); });
    try {
        if (length > sts2::resource_limits().snapshot_bytes)
            return call(h, [] { throw sts2::CapacityError("snapshot: configured document byte resource budget exceeded"); });
        std::string payload(data, length);  // last_result can be the supplied buffer
        return call(h, [&] { *successor = new Handle(sts2::Env::ImportSnapshotJson(payload)); });
    } catch (const std::bad_alloc&) {
        h.result.clear(); h.error = "native host memory resource exhausted"; return STS2_ENV_CAPACITY;
    } catch (const std::exception& error) {
        h.result.clear(); h.error = error.what(); return STS2_ENV_ERROR;
    }
}

extern "C" int sts2_env_try_transition(void* raw, sts2_action action, void** successor) {
    if (successor) *successor = nullptr;
    if (!raw || !successor) return STS2_ENV_ERROR;
    Handle& source = *static_cast<Handle*>(raw);
    return call(source, [&] {
        auto branch = std::make_unique<Handle>(source.env->Predict(action_from_c(action)));
        *successor = branch.release();
    });
}

extern "C" int sts2_env_preview(void* raw, sts2_action action, int32_t trace_limit, void** successor) {
    if (successor) *successor = nullptr;
    if (!raw || !successor) return STS2_ENV_ERROR;
    Handle& source = *static_cast<Handle*>(raw);
    return call(source, [&] {
        if (trace_limit < 0 || trace_limit > 1000000) throw sts2::InvalidRequest("preview: invalid trace limit");
        auto result = sts2::PreviewAction(*source.env, action_from_c(action),
            sts2::PreviewOptions{.collect_trace = trace_limit != 0, .trace_limit = static_cast<size_t>(trace_limit)});
        auto json = result.ExportJson();
        auto branch = std::make_unique<Handle>(std::move(result.successor));
        branch->result = std::move(json);
        *successor = branch.release();
    });
}

extern "C" int sts2_env_predict_operation(void* raw, const char* data, size_t length,
                                         int32_t trace_limit, int32_t flags, void** successor) {
    if (successor) *successor = nullptr;
    if (!raw || !successor) return STS2_ENV_ERROR;
    Handle& source = *static_cast<Handle*>(raw);
    if (!data)
        return call(source, [] { throw sts2::InvalidRequest("operation: invalid input buffer or length"); });
    try {
        if (length > sts2::resource_limits().snapshot_bytes)
            return call(source, [] { throw sts2::CapacityError("operation: configured document byte resource budget exceeded"); });
        std::string payload(data, length);  // may alias source.last_result
        return call(source, [&] {
            if (trace_limit < 0 || trace_limit > 1000000) throw sts2::InvalidRequest("operation: invalid trace limit");
            auto result = sts2::PreviewOperation(*source.env, sts2::ParseOperationJson(payload),
                preview_options(trace_limit,flags));
            auto json = result.ExportJson();
            auto branch = std::make_unique<Handle>(std::move(result.successor));
            branch->result = std::move(json);
            *successor = branch.release();
        });
    } catch (const std::bad_alloc&) {
        source.result.clear(); source.error = "native host memory resource exhausted"; return STS2_ENV_CAPACITY;
    } catch (const std::exception& error) {
        source.result.clear(); source.error = error.what(); return STS2_ENV_ERROR;
    }
}

extern "C" int sts2_env_export_combat_queries(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] { h.result = sts2::ExportCombatQueries(*h.env); });
}
extern "C" int sts2_env_export_run_queries(void* raw, int32_t flags) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] { h.result = sts2::ExportRunQueries(*h.env, query_options(flags)); });
}
extern "C" int sts2_env_preview_with_results(void* raw, sts2_action action, int32_t trace_limit,
                                              int32_t flags, void** successor) {
    if (successor) *successor = nullptr;
    if (!raw || !successor) return STS2_ENV_ERROR;
    Handle& source = *static_cast<Handle*>(raw);
    return call(source, [&] {
        if (trace_limit < 0 || trace_limit > 1000000) throw sts2::InvalidRequest("preview: invalid trace limit");
        auto result = sts2::PreviewAction(*source.env, action_from_c(action),
            preview_options(trace_limit,flags));
        auto json = result.ExportJson();
        auto branch = std::make_unique<Handle>(std::move(result.successor));
        branch->result = std::move(json);
        *successor = branch.release();
    });
}
extern "C" int sts2_env_export_calculated_vars(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] { h.result = sts2::ExportCombatCalculatedVars(*h.env); });
}
extern "C" int sts2_env_export_orbs(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] { h.result = sts2::ExportCombatOrbs(*h.env); });
}
extern "C" int sts2_env_export_powers(void* raw) {
    if (!raw) return STS2_ENV_ERROR;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] { h.result = sts2::ExportCombatPowers(*h.env); });
}

extern "C" const char* sts2_env_last_result(void* raw) {
    if (!raw) return nullptr;
    return static_cast<Handle*>(raw)->result.c_str();
}

extern "C" int32_t sts2_env_decision(void* raw) {
    if (!raw) return -1;
    Handle& h = *static_cast<Handle*>(raw);
    try { return static_cast<int32_t>(h.env->decision().kind); }
    catch (const std::exception& e) { h.error = e.what(); return -1; }
}

extern "C" uint64_t sts2_env_hash(void* raw) {
    if (!raw) return 0;
    Handle& h = *static_cast<Handle*>(raw);
    try { return h.env->Hash(); }
    catch (const std::exception& e) { h.error = e.what(); return 0; }
}

extern "C" int sts2_env_determinize(void* raw, uint64_t seed) {
    if (!raw) return -1;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] { h.env->Determinize(seed); });
}

extern "C" int sts2_env_step(void* raw, sts2_action action) {
    if (!raw) return -1;
    Handle& h = *static_cast<Handle*>(raw);
    return call(h, [&] { h.env->Step(action_from_c(action)); });
}

extern "C" int32_t sts2_env_legal_actions(void* raw, sts2_action* out, size_t capacity) {
    if (!raw) return -1;
    Handle& h = *static_cast<Handle*>(raw);
    try {
        std::vector<sts2::Action> actions = h.env->LegalActions();
        if (actions.size() > INT32_MAX) throw sts2::CapacityError("legal action count exceeds original int32 range");
        if (!out && capacity == 0) return static_cast<int32_t>(actions.size());
        if (actions.size() > capacity) { h.error = "legal action buffer too small"; return -1; }
        if (!out && !actions.empty()) { h.error = "legal action buffer is null"; return -1; }
        for (size_t i = 0; i < actions.size(); ++i) out[i] = action_to_c(actions[i]);
        return static_cast<int32_t>(actions.size());
    } catch (const std::exception& e) { h.error = e.what(); return -1; }
}

extern "C" int32_t sts2_env_action_mask(void* raw, uint8_t* out, size_t capacity) {
    if (!raw) return -1;
    Handle& h = *static_cast<Handle*>(raw);
    try {
        std::vector<uint8_t> mask = h.env->ActionMask();
        if (mask.size() > INT32_MAX) throw sts2::CapacityError("action mask count exceeds original int32 range");
        if (!out && capacity == 0) return static_cast<int32_t>(mask.size());
        if (mask.size() > capacity) { h.error = "action mask buffer too small"; return -1; }
        if (!out) { h.error = "action mask buffer is null"; return -1; }
        std::copy(mask.begin(), mask.end(), out);
        return static_cast<int32_t>(mask.size());
    } catch (const std::exception& e) { h.error = e.what(); return -1; }
}

extern "C" int32_t sts2_env_observe(void* raw, int32_t* out, size_t capacity) {
    if (!raw) return -1;
    Handle& h = *static_cast<Handle*>(raw);
    try {
        std::vector<int32_t> observation = h.env->Observe();
        if (!out && capacity == 0) return static_cast<int32_t>(observation.size());
        if (observation.size() > capacity) { h.error = "observation buffer too small"; return -1; }
        if (!out) { h.error = "observation buffer is null"; return -1; }
        std::copy(observation.begin(), observation.end(), out);
        return static_cast<int32_t>(observation.size());
    } catch (const std::exception& e) { h.error = e.what(); return -1; }
}

extern "C" const char* sts2_env_last_error(void* raw) {
    if (!raw) return "null environment handle";
    return static_cast<Handle*>(raw)->error.c_str();
}
