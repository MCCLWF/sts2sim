#include "env/api_descriptor.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "content/generated/ids.h"
#include "engine/sim/state_fields.generated.h"
#include "env/c_api.h"
#include "env/env.h"
#include "env/snapshot_json.h"

#ifndef STS2SIM_CONTENT_JSON_SHA256
#error "The API descriptor test must receive the build-time content fingerprint"
#endif
#ifndef STS2SIM_STATE_SCHEMA_HEADER_SHA256
#error "The API descriptor test must receive the build-time schema-header fingerprint"
#endif

namespace {
using sts2::snapshot_detail::Json;
using namespace std::string_view_literals;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

const Json& field(const Json& object, std::string_view key) {
    return object.at(key);
}

std::string_view string_value(const Json& value) {
    require(value.kind == Json::String, "expected a JSON string");
    return value.text;
}

bool bool_value(const Json& value) {
    require(value.kind == Json::Bool, "expected a JSON boolean");
    return value.boolean;
}

bool valid_sha256(std::string_view value) {
    if (value.size() != 64) return false;
    for (char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

template <size_t N>
void check_enum(const Json& actual, const std::array<std::pair<std::string_view, int32_t>, N>& expected) {
    require(actual.kind == Json::Array && actual.array.size() == N, "enum mapping has wrong size");
    for (size_t i = 0; i < N; ++i) {
        const Json& entry = actual.array[i];
        require(string_value(field(entry, "name")) == expected[i].first, "enum mapping name/order changed");
        require(field(entry, "value").integer<int32_t>() == expected[i].second, "enum mapping value disagrees with ABI");
    }
}

template <size_t N>
void check_strings(const Json& actual, const std::array<std::string_view, N>& expected) {
    require(actual.kind == Json::Array && actual.array.size() == N, "string list has wrong size");
    for (size_t i = 0; i < N; ++i)
        require(string_value(actual.array[i]) == expected[i], "string list is incomplete or reordered");
}

void run_contract_test() {
    static_assert(sizeof(sts2_action) == 20);
    static_assert(alignof(sts2_action) == 4);
    static_assert(offsetof(sts2_action, kind) == 0);
    static_assert(offsetof(sts2_action, index) == 4);
    static_assert(offsetof(sts2_action, target) == 8);
    static_assert(offsetof(sts2_action, col) == 12);
    static_assert(offsetof(sts2_action, row) == 16);

    require(sts2_env_abi_version() == 1, "ABI version must be 1");
    const char* descriptor_ptr = sts2_env_api_descriptor();
    require(descriptor_ptr != nullptr, "descriptor pointer is null");
    require(sts2_env_api_descriptor() == descriptor_ptr, "descriptor storage is not stable");
    const Json root = sts2::snapshot_detail::Parser(descriptor_ptr).parse();
    require(string_value(field(root, "format")) == "sts2env.api_descriptor", "descriptor format mismatch");
    require(field(root, "protocol_version").integer<int32_t>() == 2, "protocol version must be 2");
    require(field(root, "abi_version").integer<uint32_t>() == sts2_env_abi_version(), "descriptor ABI version mismatch");

    const Json& game = field(root, "game");
    require(string_value(field(game, "version")) == "v0.111.0", "game version mismatch");
    require(string_value(field(game, "release_commit")) == "41cef1ea", "game release mismatch");
    const auto content_hash = string_value(field(game, "content_json_sha256"));
    require(valid_sha256(content_hash), "content hash is not SHA-256");
    require(content_hash == STS2SIM_CONTENT_JSON_SHA256, "content hash does not match build input");
    require(field(game, "model_id_hash").integer<uint32_t>() == sts2::kModelIdHash, "model ID hash mismatch");

    const Json& snapshot = field(root, "snapshot");
    require(string_value(field(snapshot, "format")) == "sts2env.logical_state", "snapshot format mismatch");
    require(field(snapshot, "version").integer<int32_t>() == 4, "snapshot version mismatch");
    require(string_value(field(snapshot, "schema_sha256")) == sts2::kStateSchemaSha256, "snapshot schema mismatch");
    const auto header_hash = string_value(field(snapshot, "schema_header_sha256"));
    require(valid_sha256(header_hash), "schema header hash is not SHA-256");
    require(header_hash == STS2SIM_STATE_SCHEMA_HEADER_SHA256, "schema header hash does not match build input");
    require(field(snapshot, "model_id_hash").integer<uint32_t>() == sts2::kModelIdHash, "snapshot model ID hash mismatch");
    const Json& registry = field(snapshot, "runtime_registry");
    require(string_value(field(registry, "registry_scope")) == "loaded_native_runtime", "snapshot registry scope mismatch");
    require(string_value(field(registry, "runtime_id_scope")) == "loaded_library_instance", "runtime ID scope mismatch");
    require(bool_value(field(registry, "same_runtime_import_only")), "snapshot portability claim is too broad");
    require(!bool_value(field(registry, "cross_process_portable")), "snapshot must not claim cross-process portability");
    require(bool_value(field(registry, "registry_indices_runtime_bound")), "registry binding missing");
    require(bool_value(field(registry, "model_ids_bound_by_hash")), "model ID binding missing");
    require(!bool_value(field(registry, "runtime_id_is_game_rng_input")), "runtime ID must be transport-only");
    const Json& portable = field(root, "portable_snapshot");
    require(string_value(field(portable, "format")) == "sts2env.portable_snapshot", "portable format mismatch");
    require(field(portable, "version").integer<int32_t>() == 1, "portable version mismatch");
    require(valid_sha256(string_value(field(portable, "native_build_sha256"))), "portable build binding missing");
    require(bool_value(field(portable, "cross_process_portable"))
        && bool_value(field(portable, "identical_native_build_required")), "portable build restriction missing");
    require(!bool_value(field(portable, "wire_hash_is_native_source_hash")), "portable wire hash promoted to runtime source identity");

    const Json& enums = field(root, "enums");
    check_enum(field(enums, "action_kind"), std::array{
        std::pair{"start_run"sv, static_cast<int32_t>(sts2::Action::StartRun)},
        std::pair{"map_move"sv, static_cast<int32_t>(sts2::Action::MapMove)},
        std::pair{"play_card"sv, static_cast<int32_t>(sts2::Action::PlayCard)},
        std::pair{"end_turn"sv, static_cast<int32_t>(sts2::Action::EndTurn)},
        std::pair{"take_reward"sv, static_cast<int32_t>(sts2::Action::TakeReward)},
        std::pair{"proceed"sv, static_cast<int32_t>(sts2::Action::Proceed)},
        std::pair{"event_option"sv, static_cast<int32_t>(sts2::Action::EventOption)},
        std::pair{"rest_option"sv, static_cast<int32_t>(sts2::Action::RestOption)},
        std::pair{"shop_buy"sv, static_cast<int32_t>(sts2::Action::ShopBuy)},
        std::pair{"treasure_pick"sv, static_cast<int32_t>(sts2::Action::TreasurePick)},
        std::pair{"use_potion"sv, static_cast<int32_t>(sts2::Action::UsePotion)},
        std::pair{"discard_potion"sv, static_cast<int32_t>(sts2::Action::DiscardPotion)},
        std::pair{"answer"sv, static_cast<int32_t>(sts2::Action::Answer)},
        std::pair{"resume"sv, static_cast<int32_t>(sts2::Action::Resume)},
    });
    check_enum(field(enums, "decision_kind"), std::array{
        std::pair{"map"sv, static_cast<int32_t>(sts2::DecisionKind::Map)},
        std::pair{"combat"sv, static_cast<int32_t>(sts2::DecisionKind::Combat)},
        std::pair{"rewards"sv, static_cast<int32_t>(sts2::DecisionKind::Rewards)},
        std::pair{"event"sv, static_cast<int32_t>(sts2::DecisionKind::Event)},
        std::pair{"rest_site"sv, static_cast<int32_t>(sts2::DecisionKind::RestSite)},
        std::pair{"shop"sv, static_cast<int32_t>(sts2::DecisionKind::Shop)},
        std::pair{"treasure"sv, static_cast<int32_t>(sts2::DecisionKind::Treasure)},
        std::pair{"choice"sv, static_cast<int32_t>(sts2::DecisionKind::Choice)},
        std::pair{"ended"sv, static_cast<int32_t>(sts2::DecisionKind::Ended)},
        std::pair{"boundary"sv, static_cast<int32_t>(sts2::DecisionKind::Boundary)},
    });
    check_enum(field(enums, "status"), std::array{
        std::pair{"ok"sv, static_cast<int32_t>(STS2_ENV_OK)},
        std::pair{"unported"sv, static_cast<int32_t>(STS2_ENV_UNPORTED)},
        std::pair{"trace_limit"sv, static_cast<int32_t>(STS2_ENV_TRACE_LIMIT)},
        std::pair{"unsupported"sv, static_cast<int32_t>(STS2_ENV_UNSUPPORTED)},
        std::pair{"capacity"sv, static_cast<int32_t>(STS2_ENV_CAPACITY)},
        std::pair{"invalid_request"sv, static_cast<int32_t>(STS2_ENV_INVALID_REQUEST)},
        std::pair{"error"sv, static_cast<int32_t>(STS2_ENV_ERROR)},
    });

    const Json& abi = field(root, "abi");
    require(string_value(field(abi, "calling_convention")) == "C", "calling convention mismatch");
    const Json& action_struct = field(abi, "action_struct");
    require(field(action_struct, "size_bytes").integer<int32_t>() == static_cast<int32_t>(sizeof(sts2_action)), "action ABI size mismatch");
    require(field(action_struct, "alignment_bytes").integer<int32_t>() == static_cast<int32_t>(alignof(sts2_action)), "action ABI alignment mismatch");
    const Json& action_fields = field(action_struct, "fields");
    constexpr std::array<std::pair<std::string_view, int32_t>, 5> expected_action_fields{{
        {"kind", static_cast<int32_t>(offsetof(sts2_action, kind))},
        {"index", static_cast<int32_t>(offsetof(sts2_action, index))},
        {"target", static_cast<int32_t>(offsetof(sts2_action, target))},
        {"col", static_cast<int32_t>(offsetof(sts2_action, col))},
        {"row", static_cast<int32_t>(offsetof(sts2_action, row))},
    }};
    require(action_fields.kind == Json::Array && action_fields.array.size() == expected_action_fields.size(), "action fields missing");
    for (size_t i = 0; i < expected_action_fields.size(); ++i) {
        const Json& entry = action_fields.array[i];
        require(string_value(field(entry, "name")) == expected_action_fields[i].first, "action field order mismatch");
        require(string_value(field(entry, "type")) == "int32_t", "action field type mismatch");
        require(field(entry, "offset_bytes").integer<int32_t>() == expected_action_fields[i].second, "action field offset mismatch");
    }
    const Json& buffers = field(root, "buffers");
    require(string_value(field(buffers, "capacity_unit")) == "elements", "buffer capacity unit missing");
    require(bool_value(field(buffers, "capacity_checked_before_write")), "buffer capacity contract missing");
    require(bool_value(field(buffers, "undersized_buffer_is_not_partially_written")), "partial-write contract missing");
    require(bool_value(field(buffers, "caller_must_supply_valid_pointer_when_capacity_is_sufficient")), "buffer pointer contract missing");
    require(string_value(field(field(buffers, "legal_actions"), "element_type")) == "sts2_action", "legal action element type mismatch");
    require(string_value(field(field(buffers, "action_mask"), "element_type")) == "uint8_t", "action mask element type mismatch");
    require(field(field(buffers, "action_mask"), "minimum_length").integer<int32_t>() == 512, "action mask minimum mismatch");
    require(string_value(field(field(buffers, "observation"), "element_type")) == "int32_t", "observation element type mismatch");
    require(field(field(buffers, "observation"), "minimum_length").integer<int32_t>() == 2048, "observation minimum mismatch");
    require(field(field(buffers, "observation"), "schema_version").integer<int32_t>() == 2, "observation schema mismatch");
    require(bool_value(field(buffers, "null_output_zero_capacity_queries_required_length")), "buffer discovery missing");

    const Json& ownership = field(root, "ownership");
    require(string_value(field(ownership, "last_result_lifetime")).find("next API call") != std::string_view::npos, "last_result lifetime missing");
    require(string_value(field(ownership, "last_error_lifetime")).find("next API call") != std::string_view::npos, "last_error lifetime missing");
    require(string_value(field(ownership, "descriptor_lifetime")).find("loaded library instance") != std::string_view::npos, "descriptor lifetime missing");
    require(bool_value(field(ownership, "successor_output_is_null_on_failure")), "successor failure contract missing");

    const Json& semantics = field(root, "result_semantics");
    require(field(semantics, "step_failure_is_atomic").kind == Json::Bool && !bool_value(field(semantics, "step_failure_is_atomic")), "Step must document non-atomic failure");
    require(bool_value(field(semantics, "predict_failure_is_atomic_for_source_gameplay_state")), "Predict source atomicity missing");
    require(static_cast<int32_t>(STS2_ENV_UNPORTED) == 1 && static_cast<int32_t>(STS2_ENV_TRACE_LIMIT) == 2, "C status semantics changed");
    const Json& scalar_returns = field(semantics, "scalar_returns");
    require(string_value(field(scalar_returns, "decision")).find("decision_kind") != std::string_view::npos, "decision return semantics missing");
    require(string_value(field(scalar_returns, "hash")).find("uint64") != std::string_view::npos, "hash return semantics missing");
    require(string_value(field(scalar_returns, "event_rng_counter")).find("-1") != std::string_view::npos, "event RNG return semantics missing");

    const Json& preview = field(root, "preview");
    require(!bool_value(field(preview, "trace_complete")), "descriptor must not claim a complete trace");
    require(!bool_value(field(preview, "arbitrary_stop_argument")), "descriptor must not claim arbitrary stop support");
    require(field(preview, "trace_limit_max").integer<int32_t>() == 1000000, "preview trace limit mismatch");

    const Json& exports = field(root, "exports");
    require(bool_value(field(exports, "complete_for_abi_version")), "export list completeness not asserted");
    check_strings(field(exports, "symbols"), std::array<std::string_view, 36>{
        "sts2_env_abi_version", "sts2_env_api_descriptor", "sts2_env_create", "sts2_env_clone", "sts2_env_destroy",
        "sts2_env_reset", "sts2_env_reset_named", "sts2_env_reset_combat", "sts2_env_inspect_combat_named",
        "sts2_env_inspect_current", "sts2_env_export_oracle_combat_state", "sts2_env_export_oracle_run_state",
        "sts2_env_export_oracle_rewards", "sts2_env_event_rng_counter", "sts2_env_validate_combat_root",
        "sts2_env_export_snapshot", "sts2_env_import_snapshot", "sts2_env_try_transition", "sts2_env_preview",
        "sts2_env_preview_with_results", "sts2_env_export_run_queries", "sts2_env_export_combat_queries",
        "sts2_env_export_calculated_vars", "sts2_env_export_orbs", "sts2_env_export_powers", "sts2_env_last_result",
        "sts2_env_decision", "sts2_env_hash", "sts2_env_determinize", "sts2_env_step", "sts2_env_legal_actions",
        "sts2_env_action_mask", "sts2_env_observe", "sts2_env_last_error",
        "sts2_env_predict_operation",
        "sts2_env_export_portable_snapshot",
    });

    const Json& capabilities = field(root, "capabilities");
    check_strings(field(capabilities, "supported"), std::array<std::string_view, 14>{
        "run_and_combat_initialization", "decision_inspection_and_legal_actions", "single_action_step",
        "pure_single_action_prediction", "bounded_factual_action_preview", "clone_hash_and_determinize",
        "versioned_logical_snapshot_same_runtime", "agent_observation_and_action_mask",
        "selected_run_combat_and_oracle_diagnostics",
        "typed_native_operation_prediction", "prefix_replay_enemy_phase_and_safe_outcome_boundaries",
        "portable_snapshot_identical_native_build",
        "privileged_rng_call_diagnostics",
        "privileged_model_hook_call_frames",
    });
    check_strings(field(capabilities, "unsupported"), std::array<std::string_view, 5>{
        "complete_causal_execution_trace", "client_selected_arbitrary_stop_condition", "generic_live_game_capture",
        "unported_content_execution", "multiplayer_semantics",
    });
    const Json& qualifications = field(capabilities, "qualifications");
    require(qualifications.kind == Json::Array && qualifications.array.size() >= 4, "capability qualifications missing");
}
}  // namespace

int main() {
    try {
        run_contract_test();
        std::cout << "API descriptor contract passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "API descriptor contract failed: " << error.what() << '\n';
        return 1;
    }
}
