#include "env/preview.h"

#include <algorithm>
#include "engine/json.h"
#include "engine/sim/execution_value_json.h"

namespace sts2 {
namespace {
const char* action_name(Action::Kind kind) {
    constexpr const char* names[] = {"start_run", "map_move", "play_card", "end_turn",
        "take_reward", "proceed", "event_option", "rest_option", "shop_buy",
        "treasure_pick", "use_potion", "discard_potion", "answer", "resume"};
    return names[static_cast<size_t>(kind)];
}
const char* event_name(ExecutionEventKind kind) {
    constexpr const char* names[] = {"card_play_started", "card_play_finished", "damage_received",
        "creature_attacked", "card_drawn", "card_draw_resolved", "card_generated",
        "card_generation_resolved", "orb_channeled", "card_effect_started", "damage_resolved"};
    return names[static_cast<size_t>(kind)];
}
const char* source_name(ExecutionDamageSourceKind kind) {
    constexpr const char* names[] = {"unknown", "card", "potion", "power", "relic",
        "poison", "thorns", "orb", "monster_move"};
    return names[static_cast<size_t>(kind)];
}
void damage_fields(JsonWriter& w, const auto& hit) {
    w.field("blocked", hit.blocked).field("unblocked", hit.unblocked).field("overkill", hit.overkill)
        .field("props", hit.props).field("block_broken", hit.block_broken)
        .field("fully_blocked", hit.fully_blocked).field("target_killed", hit.target_killed);
}
}  // namespace

PreviewResult PreviewAction(const Env& source, const Action& action, const PreviewOptions& options) {
    if((options.collect_rng || options.collect_causal) && !options.collect_trace) throw InvalidRequest("preview: privileged diagnostics require enabled trace");
    if (options.collect_trace && (options.trace_limit == 0 || options.trace_limit > 1000000))
        throw InvalidRequest("preview: trace_limit must be between 1 and 1000000");
    if (source.sim().g->combat.is_in_progress) source.ValidateCombatRoot();
    const auto legal = source.LegalActions();
    if (std::find(legal.begin(), legal.end(), action) == legal.end())
        throw InvalidRequest("preview: action is not legal at this decision");
    PreviewResult result;
    result.source_hash = source.Hash();
    result.action = action;
    result.choice_replayed = action.kind == Action::Answer || action.kind == Action::Resume;
    if (result.choice_replayed) result.continuation_of = source.PendingOperation();
    result.trace_enabled = options.collect_trace;
    result.trace.limit = options.trace_limit;
    result.trace.collect_rng = options.collect_rng;
    result.trace.collect_causal = options.collect_causal;
    result.run_query_options = options.run_query_options;
    if (options.capture_run_results) result.result_source = source.Clone();
    result.successor = source.Clone();
    {
        ExecutionTraceScope observer(result.successor->sim(), options.collect_trace ? &result.trace : nullptr);
        // The pending operation is replayed for Answer; all frames and events
        // below this invocation describe the new complete operation prefix.
        ExecutionSourceScope invocation(result.successor->sim(), ExecutionDamageSourceKind::Unknown,
                                         "", result.choice_replayed ? "replay_operation" : action_name(action.kind));
        result.successor->Step(action);
    }
    if (result.successor->state().combat.is_in_progress) result.successor->ValidateCombatRoot();
    if (source.Hash() != result.source_hash) throw GameError("preview: source changed during observation");
    return result;
}

PreviewResult PreviewOperation(const Env& source, const Operation& operation, const PreviewOptions& options) {
    if((options.collect_rng || options.collect_causal) && !options.collect_trace) throw InvalidRequest("preview: privileged diagnostics require enabled trace");
    ValidateOperation(source, operation);
    if (operation.kind == Operation::Kind::Action
        && (operation.action.kind == Action::Answer || operation.action.kind == Action::Resume)) {
        auto result = PreviewAction(source, operation.action, options);
        result.operation = operation;
        return result;
    }
    if (options.collect_trace && (options.trace_limit == 0 || options.trace_limit > 1000000))
        throw InvalidRequest("preview: trace_limit must be between 1 and 1000000");
    if (source.sim().g->combat.is_in_progress) source.ValidateCombatRoot();
    PreviewResult result;
    result.source_hash = source.Hash();
    result.operation = operation;
    result.action = operation.action;
    if (operation.kind == Operation::Kind::GenerateRewards) result.continuation_of = source.PendingOperation();
    result.trace_enabled = options.collect_trace;
    result.trace.limit = options.trace_limit;
    result.trace.collect_rng = options.collect_rng;
    result.trace.collect_causal = options.collect_causal;
    result.run_query_options = options.run_query_options;
    if (options.capture_run_results) result.result_source = source.Clone();
    result.successor = source.Clone();
    {
        ExecutionTraceScope observer(result.successor->sim(), options.collect_trace ? &result.trace : nullptr);
        ExecutionSourceScope invocation(result.successor->sim(), ExecutionDamageSourceKind::Unknown,
                                         "", operation.kind == Operation::Kind::Action
                                             ? action_name(operation.action.kind) : OperationName(operation.kind));
        result.successor->Execute(operation);
    }
    if (result.successor->state().combat.is_in_progress) result.successor->ValidateCombatRoot();
    if (source.Hash() != result.source_hash) throw GameError("preview: source changed during native operation");
    return result;
}

std::string PreviewResult::ExportJson() const {
    if (!successor) throw GameError("preview: no successor");
    const auto decision = successor->decision();
    const char* reason = decision.kind == DecisionKind::Choice ? "choice"
        : decision.kind == DecisionKind::Ended ? "ended"
        : decision.boundary ? (decision.boundary->kind == ExecutionBoundaryKind::EnemyPhaseDone
            ? "enemy_phase_done" : "safe_outcome") : "decision";
    JsonWriter w;
    w.begin_object().field("format", "sts2env.preview").field("version", result_source ? 2 : 1)
        .field("source_hash", source_hash).field("successor_hash", successor->Hash())
        .field("operation_status", decision.kind == DecisionKind::Choice ? "needs_choice"
            : decision.boundary ? "paused" : "complete")
        .field("decision", static_cast<int32_t>(decision.kind)).field("stop_reason", reason)
        .field("trace_enabled", trace_enabled).field("trace_semantics", "operation_prefix")
        .field("choice_replayed", choice_replayed).field("trace_complete", false)
        .field("causal_trace_enabled",trace.collect_causal).field("causal_trace_visibility","privileged_diagnostic")
        .field("trace_limit", static_cast<uint64_t>(trace.limit));
    w.key("action").begin_object().field("kind", static_cast<int32_t>(action.kind))
        .field("name", action_name(action.kind)).field("index", action.index).field("target", action.target)
        .field("col", action.coord.col).field("row", action.coord.row).end_object();
    if (operation) { w.key("operation"); WriteOperation(w, *operation); }
    if (continuation_of) { w.key("continuation_of"); WriteOperation(w, *continuation_of); }
    w.key("operation_result");
    if (const auto& purchase = successor->LastOperationResult().merchant_purchase) {
        w.begin_object().field("kind", "merchant_purchase").field("entry_index", purchase->entry_index)
            .field("success", purchase->success).field("ignore_cost", purchase->ignore_cost).end_object();
    } else w.null();
    if (decision.boundary) w.key("boundary").begin_object()
        .field("kind", static_cast<int32_t>(decision.boundary->kind))
        .field("ordinal", decision.boundary->ordinal).end_object();
    w.key("frames").begin_array();
    for (const auto& frame : trace.frames) {
        w.begin_object().field("parent", frame.parent).field("operation", frame.operation)
            .field("source_kind", source_name(frame.source_kind)).field("source_id", frame.source_id)
            .field("kind",frame.kind).field("source_model_kind",frame.source_model_kind)
            .field("source_model_id",frame.source_model_id).field("source_ref",frame.source_ref)
            .field("source_ref_kind",frame.source_ref_kind).field("source_is_override",frame.source_is_override)
            .field("source_is_canonical",frame.source_is_canonical)
            .field("source_is_canonical_known",frame.source_is_canonical_known)
            .field("sequence",frame.sequence).field("finished_sequence",frame.finished_sequence)
            .field("finished",frame.finished).field("unwound",frame.unwound);
        WriteExecutionMethodValues(w,frame);w.end_object();
    }
    w.end_array().key("events").begin_array();
    for (const auto& event : trace.events) {
        w.begin_object().field("kind", event_name(event.kind)).field("sequence", event.sequence)
            .field("frame", event.frame).field("card_ref", event.card).field("card_id", event.card_id)
            .field("owner_ref", event.owner).field("creature_ref", event.creature)
            .field("dealer_ref", event.dealer).field("creature_combat_id", static_cast<uint64_t>(event.creature_combat_id))
            .field("dealer_combat_id", static_cast<uint64_t>(event.dealer_combat_id))
            .field("original_event_index", event.original_index).field("hit_count", event.hit_count)
            .field("source_kind", source_name(event.damage_source_kind)).field("source_id", event.damage_source_id);
        damage_fields(w, event);
        if (event.kind == ExecutionEventKind::CardPlayFinished) w.field("ethereal", event.flag);
        if (event.kind == ExecutionEventKind::CardDrawn) w.field("from_hand", event.flag);
        if (event.kind == ExecutionEventKind::OrbChanneled) w.field("lightning", event.flag);
        w.key("card_facts");
        if (event.card_facts) {
            const auto& card = *event.card_facts;
            w.begin_object().field("owner", card.owner).field("upgrade", card.upgrade).field("type", card.type).end_object();
        } else w.null();
        w.key("play_facts");
        if (event.play_facts) {
            const auto& play = *event.play_facts;
            w.begin_object().field("target", play.target).field("target_type", play.target_type)
                .field("result_pile", play.result_pile).field("energy_spent", play.energy_spent)
                .field("energy_value", play.energy_value).field("stars_spent", play.stars_spent)
                .field("star_value", play.star_value).field("auto_play", play.auto_play)
                .field("play_index", play.play_index).field("play_count", play.play_count).end_object();
        } else w.null();
        w.key("hits").begin_array();
        for (const auto& hit : event.hit_results) {
            w.begin_object().field("receiver_ref", hit.receiver); damage_fields(w, hit); w.end_object();
        }
        w.end_array().end_object();
    }
    w.end_array().key("power_events").begin_array();
    for (const auto& event : trace.power_events) {
        constexpr const char* kinds[] = {"amount_changed", "attached", "removed"};
        w.begin_object().field("kind", kinds[static_cast<size_t>(event.kind)]).field("sequence", event.sequence)
            .field("frame", event.frame).field("power_ref", event.power).field("power_id", event.power_id)
            .field("owner_ref", event.owner).field("owner_combat_id", static_cast<uint64_t>(event.owner_combat_id))
            .field("previous_amount", event.previous_amount).field("amount", event.amount)
            .field("attached", event.attached).end_object();
    }
    w.end_array().field("rng_trace_enabled",trace.collect_rng)
        .field("rng_trace_visibility","privileged_diagnostic").key("rng_calls").begin_array();
    auto value=[&](const RngTraceValue& v) {
        constexpr const char* types[]={"void","bool","int32","uint32","uint64","float32","float64"};
        w.begin_object().field("type",types[static_cast<size_t>(v.type)]).field("bits",v.bits).end_object();
    };
    auto rng_state=[&](const RngTraceState& s) {
        w.begin_object().field("counter",s.counter).key("state").begin_array();
        for(auto bits:s.state) w.value(bits);
        w.end_array().end_object();
    };
    for(size_t index=0;index<trace.rng_calls.size();++index) {
        const auto& call=trace.rng_calls[index];
        w.begin_object().field("call",static_cast<uint64_t>(index)).field("parent_call",call.parent_call)
            .field("frame",call.frame).field("operation",call.operation).field("sequence",call.sequence)
            .field("finished_sequence",call.finished_sequence).field("finished",call.finished)
            .field("scope",call.scope).field("stream",call.stream).field("owner_ref",call.owner_ref)
            .field("owner_combat_id",static_cast<uint64_t>(call.owner_combat_id)).field("owner_model_id",call.owner_model_id)
            .key("parameters").begin_array();for(const auto& parameter:call.parameters) value(parameter);
        w.end_array().key("weight_bits").begin_array();for(auto bits:call.weight_bits) w.value(uint64_t(bits));
        w.end_array().key("before");rng_state(call.before);w.key("after");rng_state(call.after);
        w.key("result");value(call.result);w.end_object();
    }
    w.end_array();
    if (result_source) {
        w.key("run_results").begin_object().field("semantics", "source_to_successor_projection")
            .key("before");
        WriteRunQueries(w, *result_source, run_query_options);
        w.key("after");
        WriteRunQueries(w, *successor, run_query_options);
        w.end_object();
    }
    w.end_object();
    return w.str();
}
}  // namespace sts2
