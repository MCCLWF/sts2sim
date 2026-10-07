#include "env/api_descriptor.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "content/generated/ids.h"
#include "engine/json.h"
#include "engine/sim/state_fields.generated.h"
#include "env/c_api.h"
#include "env_build_identity.h"
#include "env/portable_registry.h"
#include "env/env.h"
#include "env/resource_limits.h"

#ifndef STS2SIM_CONTENT_JSON_SHA256
#error "STS2SIM_CONTENT_JSON_SHA256 must be set from data/content.json at build time"
#endif
#ifndef STS2SIM_STATE_SCHEMA_HEADER_SHA256
#error "STS2SIM_STATE_SCHEMA_HEADER_SHA256 must be set from state_fields.generated.h at build time"
#endif

namespace {

struct EnumEntry {
    std::string_view name;
    int32_t value;
};

constexpr std::array kActions{
    EnumEntry{"start_run", static_cast<int32_t>(sts2::Action::StartRun)},
    EnumEntry{"map_move", static_cast<int32_t>(sts2::Action::MapMove)},
    EnumEntry{"play_card", static_cast<int32_t>(sts2::Action::PlayCard)},
    EnumEntry{"end_turn", static_cast<int32_t>(sts2::Action::EndTurn)},
    EnumEntry{"take_reward", static_cast<int32_t>(sts2::Action::TakeReward)},
    EnumEntry{"proceed", static_cast<int32_t>(sts2::Action::Proceed)},
    EnumEntry{"event_option", static_cast<int32_t>(sts2::Action::EventOption)},
    EnumEntry{"rest_option", static_cast<int32_t>(sts2::Action::RestOption)},
    EnumEntry{"shop_buy", static_cast<int32_t>(sts2::Action::ShopBuy)},
    EnumEntry{"treasure_pick", static_cast<int32_t>(sts2::Action::TreasurePick)},
    EnumEntry{"use_potion", static_cast<int32_t>(sts2::Action::UsePotion)},
    EnumEntry{"discard_potion", static_cast<int32_t>(sts2::Action::DiscardPotion)},
    EnumEntry{"answer", static_cast<int32_t>(sts2::Action::Answer)},
    EnumEntry{"resume", static_cast<int32_t>(sts2::Action::Resume)},
};

constexpr std::array kDecisions{
    EnumEntry{"map", static_cast<int32_t>(sts2::DecisionKind::Map)},
    EnumEntry{"combat", static_cast<int32_t>(sts2::DecisionKind::Combat)},
    EnumEntry{"rewards", static_cast<int32_t>(sts2::DecisionKind::Rewards)},
    EnumEntry{"event", static_cast<int32_t>(sts2::DecisionKind::Event)},
    EnumEntry{"rest_site", static_cast<int32_t>(sts2::DecisionKind::RestSite)},
    EnumEntry{"shop", static_cast<int32_t>(sts2::DecisionKind::Shop)},
    EnumEntry{"treasure", static_cast<int32_t>(sts2::DecisionKind::Treasure)},
    EnumEntry{"choice", static_cast<int32_t>(sts2::DecisionKind::Choice)},
    EnumEntry{"ended", static_cast<int32_t>(sts2::DecisionKind::Ended)},
    EnumEntry{"boundary", static_cast<int32_t>(sts2::DecisionKind::Boundary)},
};

constexpr std::array kStatuses{
    EnumEntry{"ok", STS2_ENV_OK},
    EnumEntry{"unported", STS2_ENV_UNPORTED},
    EnumEntry{"trace_limit", STS2_ENV_TRACE_LIMIT},
    EnumEntry{"unsupported", STS2_ENV_UNSUPPORTED},
    EnumEntry{"capacity", STS2_ENV_CAPACITY},
    EnumEntry{"invalid_request", STS2_ENV_INVALID_REQUEST},
    EnumEntry{"error", STS2_ENV_ERROR},
};

template <size_t N>
void write_enum(sts2::JsonWriter& writer, const std::array<EnumEntry, N>& entries) {
    writer.begin_array();
    for (const EnumEntry& entry : entries) {
        writer.begin_object().field("name", entry.name).field("value", entry.value).end_object();
    }
    writer.end_array();
}

template <size_t N>
void write_strings(sts2::JsonWriter& writer, const std::array<std::string_view, N>& strings) {
    writer.begin_array();
    for (std::string_view value : strings) writer.value(value);
    writer.end_array();
}

std::string make_descriptor() {
    constexpr std::array<std::string_view, 36> exports{
        "sts2_env_abi_version", "sts2_env_api_descriptor",
        "sts2_env_create", "sts2_env_clone", "sts2_env_destroy",
        "sts2_env_reset", "sts2_env_reset_named", "sts2_env_reset_combat",
        "sts2_env_inspect_combat_named", "sts2_env_inspect_current",
        "sts2_env_export_oracle_combat_state", "sts2_env_export_oracle_run_state",
        "sts2_env_export_oracle_rewards", "sts2_env_event_rng_counter",
        "sts2_env_validate_combat_root", "sts2_env_export_snapshot", "sts2_env_import_snapshot",
        "sts2_env_try_transition", "sts2_env_preview", "sts2_env_preview_with_results",
        "sts2_env_export_run_queries", "sts2_env_export_combat_queries",
        "sts2_env_export_calculated_vars", "sts2_env_export_orbs", "sts2_env_export_powers",
        "sts2_env_last_result", "sts2_env_decision", "sts2_env_hash", "sts2_env_determinize",
        "sts2_env_step", "sts2_env_legal_actions", "sts2_env_action_mask",
        "sts2_env_observe", "sts2_env_last_error",
        "sts2_env_predict_operation",
        "sts2_env_export_portable_snapshot",
    };
    constexpr std::array<std::string_view, 14> supported{
        "run_and_combat_initialization",
        "decision_inspection_and_legal_actions",
        "single_action_step",
        "pure_single_action_prediction",
        "bounded_factual_action_preview",
        "clone_hash_and_determinize",
        "versioned_logical_snapshot_same_runtime",
        "agent_observation_and_action_mask",
        "selected_run_combat_and_oracle_diagnostics",
        "typed_native_operation_prediction",
        "prefix_replay_enemy_phase_and_safe_outcome_boundaries",
        "portable_snapshot_identical_native_build",
        "privileged_rng_call_diagnostics",
        "privileged_model_hook_call_frames",
    };
    constexpr std::array<std::string_view, 5> unsupported{
        "complete_causal_execution_trace",
        "client_selected_arbitrary_stop_condition",
        "generic_live_game_capture",
        "unported_content_execution",
        "multiplayer_semantics",
    };

    sts2::JsonWriter writer;
    writer.begin_object()
        .field("format", "sts2env.api_descriptor")
        .field("protocol_version", 2)
        .field("abi_version", 1);

    writer.key("game").begin_object()
        .field("version", "v0.111.0")
        .field("release_commit", "41cef1ea")
        .field("content_json_sha256", STS2SIM_CONTENT_JSON_SHA256)
        .field("model_id_hash", static_cast<uint64_t>(sts2::kModelIdHash))
        .end_object();

    writer.key("portable_snapshot").begin_object()
        .field("format","sts2env.portable_snapshot").field("version",1)
        .field("native_build_sha256",sts2::kNativeBuildSha256)
        .field("native_runtime_abi",sts2::snapshot_detail::native_runtime_abi())
        .field("cross_process_portable",true).field("identical_native_build_required",true)
        .field("matching_math_runtime_and_float_environment_required",true)
        .field("canonical_registry_indices",true).field("original_machine_state_order_preserved",true)
        .field("local_source_hash_rebound_on_restore",true)
        .field("wire_hash_is_native_source_hash",false)
        .field("cross_build_migration",false).field("live_game_capture",false).end_object();

    writer.key("snapshot").begin_object()
        .field("format", "sts2env.logical_state")
        .field("version", 4)
        .field("schema_sha256", sts2::kStateSchemaSha256)
        .field("schema_header_sha256", STS2SIM_STATE_SCHEMA_HEADER_SHA256)
        .field("model_id_hash", static_cast<uint64_t>(sts2::kModelIdHash))
        .key("runtime_registry").begin_object()
        .field("registry_scope", "loaded_native_runtime")
        .field("runtime_id_scope", "loaded_library_instance")
        .field("same_runtime_import_only", true)
        .field("cross_process_portable", false)
        .field("registry_indices_runtime_bound", true)
        .field("model_ids_bound_by_hash", true)
        .field("runtime_id_is_game_rng_input", false)
        .end_object()
        .end_object();

    writer.key("abi").begin_object()
        .field("calling_convention", "C")
        .field("integer_width_bits", 32)
        .key("action_struct").begin_object()
        .field("name", "sts2_action")
        .field("size_bytes", static_cast<int32_t>(sizeof(sts2_action)))
        .field("alignment_bytes", static_cast<int32_t>(alignof(sts2_action)))
        .key("fields").begin_array()
        .begin_object().field("name", "kind").field("type", "int32_t").field("offset_bytes", static_cast<int32_t>(offsetof(sts2_action, kind))).field("semantics", "action_kind enum value").end_object()
        .begin_object().field("name", "index").field("type", "int32_t").field("offset_bytes", static_cast<int32_t>(offsetof(sts2_action, index))).field("semantics", "action-dependent card, reward, option, potion, or choice index").end_object()
        .begin_object().field("name", "target").field("type", "int32_t").field("offset_bytes", static_cast<int32_t>(offsetof(sts2_action, target))).field("semantics", "target combat ID for targeted actions; -1 when no target").end_object()
        .begin_object().field("name", "col").field("type", "int32_t").field("offset_bytes", static_cast<int32_t>(offsetof(sts2_action, col))).field("semantics", "map column for MapMove").end_object()
        .begin_object().field("name", "row").field("type", "int32_t").field("offset_bytes", static_cast<int32_t>(offsetof(sts2_action, row))).field("semantics", "map row for MapMove").end_object()
        .end_array().end_object()
        .end_object();

    writer.key("enums").begin_object();
    writer.key("action_kind"); write_enum(writer, kActions);
    writer.key("decision_kind"); write_enum(writer, kDecisions);
    writer.key("status"); write_enum(writer, kStatuses);
    writer.end_object();

    writer.key("buffers").begin_object()
        .key("legal_actions").begin_object()
        .field("element_type", "sts2_action")
        .field("length", "decision_dependent")
        .field("return", "element_count_or_minus_one")
        .field("pointer", "caller_owned_writable_storage")
        .end_object()
        .key("action_mask").begin_object()
        .field("element_type", "uint8_t")
        .field("length", "max(512, legal_action_count)")
        .field("minimum_length", 512)
        .field("return", "element_count_or_minus_one")
        .field("pointer", "caller_owned_writable_storage")
        .end_object()
        .key("observation").begin_object()
        .field("element_type", "int32_t")
        .field("length", "decision_dependent")
        .field("minimum_length", 2048)
        .field("schema_version", 2)
        .field("return", "element_count_or_minus_one")
        .field("pointer", "caller_owned_writable_storage")
        .end_object()
        .field("capacity_unit", "elements")
        .field("null_output_zero_capacity_queries_required_length", true)
        .field("capacity_checked_before_write", true)
        .field("undersized_buffer_is_not_partially_written", true)
        .field("caller_must_supply_valid_pointer_when_capacity_is_sufficient", true)
        .end_object();

    writer.key("resource_limits").begin_object()
        .field("snapshot_bytes", static_cast<uint64_t>(sts2::resource_limits().snapshot_bytes))
        .field("json_nodes", static_cast<uint64_t>(sts2::resource_limits().json_nodes))
        .field("restored_storage_bytes", static_cast<uint64_t>(sts2::resource_limits().restored_storage_bytes))
        .field("configuration", "STS2_ENV_MAX_SNAPSHOT_BYTES, STS2_ENV_MAX_JSON_NODES, STS2_ENV_MAX_RESTORED_STORAGE_BYTES; positive integers before loading the library")
        .field("limits_are_game_rules", false).end_object();

    writer.key("ownership").begin_object()
        .field("handles", "create, clone, import_snapshot, try_transition, and preview return caller-owned handles; release with sts2_env_destroy")
        .field("successor_output_is_null_on_failure", true)
        .field("last_result_lifetime", "borrowed; valid until the next API call on that handle or handle destruction")
        .field("last_error_lifetime", "borrowed; valid until the next API call on that handle or handle destruction")
        .field("descriptor_lifetime", "immutable; valid for the lifetime of the loaded library instance")
        .end_object();

    writer.key("result_semantics").begin_object()
        .field("status_codes_apply_to", "reset, inspect, snapshot import/export, root validation, transition, preview, step, and determinize")
        .field("ok", static_cast<int32_t>(STS2_ENV_OK))
        .field("unported", static_cast<int32_t>(STS2_ENV_UNPORTED))
        .field("trace_limit", static_cast<int32_t>(STS2_ENV_TRACE_LIMIT))
        .field("unsupported", static_cast<int32_t>(STS2_ENV_UNSUPPORTED))
        .field("capacity", static_cast<int32_t>(STS2_ENV_CAPACITY))
        .field("invalid_request", static_cast<int32_t>(STS2_ENV_INVALID_REQUEST))
        .field("error", static_cast<int32_t>(STS2_ENV_ERROR))
        .field("count_functions", "legal_actions, action_mask, and observe return nonnegative element counts or -1")
        .field("step_failure_is_atomic", false)
        .field("step_failure_note", "a failed mutating step can leave gameplay state partially changed; discard that branch")
        .field("predict_failure_is_atomic_for_source_gameplay_state", true)
        .field("predict_failure_note", "try_transition and preview clone before stepping; failure returns no successor and leaves source gameplay state unchanged")
        .key("scalar_returns").begin_object()
        .field("decision", "decision_kind integer; -1 on null handle or error")
        .field("hash", "uint64 logical-state hash; 0 on null handle or error")
        .field("event_rng_counter", "active event RNG counter; -1 if no active event or null handle")
        .field("last_result", "borrowed result string; JSON after export/preview calls, otherwise may be empty or retain a prior value depending on the function; null on null handle")
        .field("last_error", "borrowed stored error text; some count queries leave it unchanged; null handle returns a static diagnostic")
        .end_object()
        .end_object();

    writer.key("preview").begin_object()
        .field("operation_scope", "one legal Env action through the resulting decision or choice boundary")
        .field("trace_limit_zero_disables_collection", true)
        .field("trace_limit_max", 1000000)
        .field("trace_complete", false)
        .field("query_flags_future_bit", 1)
        .field("query_flags_rng_bit", 2)
        .field("rng_requires_enabled_trace", true)
        .field("rng_visibility", "privileged_diagnostic")
        .field("rng_values_are_typed_bit_patterns", true)
        .field("rng_local_lifetimes_classified", false)
        .field("query_flags_causal_bit", 4)
        .field("causal_requires_enabled_trace", true)
        .field("causal_visibility", "privileged_diagnostic")
        .field("causal_method_value_observation", true)
        .field("causal_method_value_scope", "generated_model_dispatch_and_hook")
        .field("causal_method_direction_metadata", "shipped_dll_reflection")
        .field("causal_model_reference_scheme", "canonical_definition_or_state_storage_ref; mutable refs have no lifetime UID")
        .field("causal_canonical_template_handles", "CanonicalCard, CanonicalRelic, EventModel.Canonical")
        .field("causal_task_values", "awaited_completion_or_result; no Task object identity")
        .field("causal_model_parameters_and_returns_complete", false)
        .field("causal_object_lifetimes_complete", false)
        .field("arbitrary_stop_argument", false)
        .field("choice_answer_replays_operation_prefix", true)
        .key("operation_statuses").begin_array().value("complete").value("needs_choice").value("paused").end_array()
        .end_object();

    writer.key("typed_operations").begin_object().field("format", "sts2env.operation").field("version", 1)
        .field("source_hash_required", true).field("client_supplied_rng_state", false)
        .key("kinds").begin_array().value("action").value("obtain_relic").value("transform_cards")
        .value("reroll_card_reward").value("merchant_purchase").value("generate_rewards").end_array()
        .key("stops").begin_array().value("operation_done").value("enemy_phase_done")
        .value("safe_outcome").value("enemy_phase_or_outcome").end_array()
        .field("generate_rewards_requires_winning_outcome_boundary", true)
        .field("no_stop_reached_returns_completed_operation", true)
        .field("boundary_resume_action", static_cast<int32_t>(sts2::Action::Resume))
        .field("rf_full_matrix_verified", false).end_object();

    writer.key("exports").begin_object()
        .field("complete_for_abi_version", true)
        .key("symbols"); write_strings(writer, exports);
    writer.end_object();

    writer.key("capabilities").begin_object()
        .key("supported"); write_strings(writer, supported);
    writer.key("unsupported"); write_strings(writer, unsupported);
    writer.key("qualifications").begin_array()
        .value("Execution support is limited to implemented content; unported transitions return STS2_ENV_UNPORTED.")
        .value("Combat-root validation is conservative and does not prove future generated content is supported.")
        .value("Only fully unlocked veteran profiles are supported; snapshot import rejects other profiles.")
        .value("Preview emits selected factual event families only and marks trace_complete false.")
        .value("RNG call diagnostics require explicit preview flag 2 and an enabled trace; local/copied streams remain unclassified.")
        .value("Model/Hook method values require explicit preview flag 4; hidden values are privileged. Generated dispatch and Hook entries are observed, while arbitrary callbacks, native Task objects, and object lifetimes remain incomplete.")
        .value("Snapshots include internal logical state and are diagnostic/transport data, not agent observations.")
        .end_array()
        .end_object()
        .end_object();

    return writer.str();
}

}  // namespace

extern "C" uint32_t sts2_env_abi_version() { return 1; }

extern "C" const char* sts2_env_api_descriptor() {
    try { static const std::string descriptor = make_descriptor(); return descriptor.c_str(); }
    catch (...) { return nullptr; }
}
